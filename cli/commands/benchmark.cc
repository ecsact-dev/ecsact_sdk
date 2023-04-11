#include "./benchmark.hh"

#include <iostream>
#include <string>
#include <cassert>
#include <cstdio>
#include <filesystem>
#include <chrono>
#include <ranges>
#include <variant>
#include <boost/dll/shared_library.hpp>
#include <boost/dll/library_info.hpp>
#include "docopt.h"
#include "nlohmann/json.hpp"
#include "ecsact/runtime/core.h"
#include "ecsact/runtime/serialize.h"
#include "ecsactsi_wasm.h"
#include "magic_enum.hpp"

using std::chrono::duration;
using std::chrono::duration_cast;
using std::chrono::nanoseconds;

namespace fs = std::filesystem;

constexpr auto USAGE = R"(Ecsact Benchmark Command

Usage:
	ecsact benchmark --runtime=<path> --seed=<path> <system_impl>...
)";

constexpr auto OPTIONS = R"(Ecsact Benchmark Command
Options:
	--runtime=<path>
		Path to built Ecsact Runtime typically one from ecsact_rtb.
	--seed=<path>
		Path to file containing entity seed data from an ecsact_dump_entities
		call. The format must be compatible with the runtime because
		ecsact_restore_entities will be called with said data.
	<system_impl>
		Path to Ecsact system implementation binaries. If the meta module is not
		available on your runtime binary you must specify the system export name
		and system ID in this format: `path;export-name,id`. Multiple exports
		may be added in semi-colon (;) deliminated list.
		NOTE: Only WebAssembly is allowed at this time.
)";

struct info_message {
	static constexpr auto type = "info";
	std::string           content;

	NLOHMANN_DEFINE_TYPE_INTRUSIVE(info_message, content);
};

struct warning_message {
	static constexpr auto type = "warning";
	std::string           content;

	NLOHMANN_DEFINE_TYPE_INTRUSIVE(warning_message, content);
};

struct error_message {
	static constexpr auto type = "error";
	std::string           content;

	NLOHMANN_DEFINE_TYPE_INTRUSIVE(error_message, content);
};

struct benchmark_progress_message {
	static constexpr auto type = "progress";

	/**
	 * 0 - 1 where 1 is 100%
	 */
	float progress;

	NLOHMANN_DEFINE_TYPE_INTRUSIVE(benchmark_progress_message, progress);
};

struct benchmark_result_message {
	static constexpr auto type = "result";
	float                 total_duration_ms;
	float                 average_duration_ms;

	NLOHMANN_DEFINE_TYPE_INTRUSIVE(
		benchmark_result_message,
		total_duration_ms,
		average_duration_ms
	);
};

using benchmark_message_variant_t = std::variant<
	info_message,
	warning_message,
	error_message,
	benchmark_progress_message,
	benchmark_result_message>;

class stdout_json_benchmark_reporter {
	template<typename MessageT>
	void _report(MessageT& message) {
		auto message_json = "{}"_json;
		to_json(message_json, message);
		message_json["type"] = MessageT::type;
		std::cout << message_json.dump() + "\n";
	}

public:
	void report(benchmark_message_variant_t message) {
		std::visit([this](auto& message) { _report(message); }, message);
	}
};

template<typename T>
static auto get_or_exit(
	boost::dll::shared_library& runtime,
	const std::string&          fn_name
) -> T& {
	if(!runtime.has(fn_name)) {
		std::cerr << "Runtime missing method: " << fn_name << "\n";
		std::exit(1);
	}

	return runtime.get<T>(fn_name);
}

static auto exists_or_exit(const fs::path& p) -> void {
	if(!fs::exists(p)) {
		std::cerr << "Cannot find path: " << p.string() << "\n";
		std::exit(1);
	}
}

struct system_impl_binary_arg {
	static auto parse(const std::string& str) -> system_impl_binary_arg {
		auto parsed_arg = system_impl_binary_arg{};

		auto semi_colon_index = str.find(';');

		parsed_arg.path = str.substr(0, semi_colon_index);

		while(semi_colon_index != std::string::npos) {
			auto next_semi_colon_index = str.find(';', semi_colon_index + 1);
			auto comma_index = str.find(',');
			if(comma_index != std::string::npos) {
				auto& export_name = parsed_arg.export_names.emplace_back();
				export_name =
					str.substr(semi_colon_index + 1, comma_index - semi_colon_index - 1);

				auto& system_id = parsed_arg.system_ids.emplace_back();
				system_id = static_cast<ecsact_system_like_id>(std::atoi(
					str.substr(comma_index + 1, next_semi_colon_index - comma_index - 1)
						.c_str()
				));
			}

			semi_colon_index = next_semi_colon_index;
		}

		return parsed_arg;
	}

	fs::path                           path;
	std::vector<std::string>           export_names;
	std::vector<ecsact_system_like_id> system_ids;
};

static auto print_last_error_if_available(
	boost::dll::shared_library&     runtime,
	stdout_json_benchmark_reporter& reporter
) -> bool {
	if(!runtime.has("ecsactsi_wasm_last_error_message")) {
		reporter.report(warning_message{
			"Cannot get wasm error message because "
			"'ecsactsi_wasm_last_error_message' is missing",
		});
		return false;
	}
	if(!runtime.has("ecsactsi_wasm_last_error_message_length")) {
		reporter.report(warning_message{
			"Cannot get wasm error message because "
			"'ecsactsi_wasm_last_error_message_length' is missing",
		});
		return false;
	}

	auto get_last_error_message_fn =
		runtime.get<decltype(ecsactsi_wasm_last_error_message)>(
			"ecsactsi_wasm_last_error_message"
		);
	auto get_last_error_message_length_fn =
		runtime.get<decltype(ecsactsi_wasm_last_error_message_length)>(
			"ecsactsi_wasm_last_error_message_length"
		);

	auto err_msg = std::string{};
	err_msg.resize(get_last_error_message_length_fn());

	if(err_msg.empty()) {
		std::cerr << "[WARNING] No additional error messages available\n";
		return false;
	}

	get_last_error_message_fn(
		err_msg.data(),
		static_cast<int32_t>(err_msg.size())
	);

	std::cerr << err_msg << "\n";

	return true;
}

int ecsact::cli::detail::benchmark_command(int argc, char* argv[]) {
	using clock_t = std::chrono::high_resolution_clock;

	auto args = docopt::docopt(USAGE, {argv + 1, argv + argc});
	auto runtime_path = args["--runtime"].asString();
	auto seed_path = args["--seed"].asString();
	auto system_impl_binaries =
		args["<system_impl>"].asStringList() |
		std::views::transform( //
			[](auto& str) { return system_impl_binary_arg::parse(str); }
		);
	auto reporter = stdout_json_benchmark_reporter{};

	auto ec = boost::system::error_code{};
	auto runtime = boost::dll::shared_library();

	exists_or_exit(runtime_path);
	exists_or_exit(seed_path);

	runtime.load(runtime_path, ec);
	if(ec) {
		std::cerr //
			<< "Failed to load runtime " << runtime_path << ": " << ec.message()
			<< "\n";
		return ec.value();
	}

	const auto create_reg_fn = get_or_exit<decltype(ecsact_create_registry)>(
		runtime,
		"ecsact_create_registry"
	);
	const auto exec_systems_fn = get_or_exit<decltype(ecsact_execute_systems)>(
		runtime,
		"ecsact_execute_systems"
	);
	const auto restore_fn = get_or_exit<decltype(ecsact_restore_entities)>(
		runtime,
		"ecsact_restore_entities"
	);
	const auto wasm_load_file_fn = get_or_exit<decltype(ecsactsi_wasm_load_file)>(
		runtime,
		"ecsactsi_wasm_load_file"
	);

	for(auto system_impl_binary : system_impl_binaries) {
		exists_or_exit(system_impl_binary.path);

		auto& system_ids = system_impl_binary.system_ids;
		auto& export_names = system_impl_binary.export_names;

		auto export_names_c = std::vector<const char*>{};
		export_names_c.reserve(export_names.size());

		for(auto& export_name : export_names) {
			export_names_c.push_back(export_name.c_str());
		}

		auto system_impl_binary_path_str = system_impl_binary.path.string();

		auto err = wasm_load_file_fn(
			system_impl_binary_path_str.c_str(),
			static_cast<int32_t>(system_ids.size()),
			system_ids.data(),
			export_names_c.data()
		);

		if(err != ECSACTSI_WASM_OK) {
			std::cerr //
				<< "Failed to load Wasm File: " << magic_enum::enum_name(err) << "\n";
			print_last_error_if_available(runtime, reporter);
			return 1;
		}
	}

	auto seed_file = std::fopen(seed_path.c_str(), "r");
	if(seed_file == nullptr) {
		std::cerr << "Failed to open seed path: " << seed_path << "\n";
		return 1;
	}

	auto reg_id = create_reg_fn("BenchmarkRegistry");

	auto restore_err = restore_fn(
		reg_id,
		[](void* out_data, int32_t data_max_length, void* ud) -> int32_t {
			return std::fread(out_data, 1, data_max_length, static_cast<FILE*>(ud));
		},
		nullptr,
		seed_file
	);

	if(restore_err != ECSACT_RESTORE_OK) {
		std::cerr //
			<< "Seed entities failed to restore: "
			<< magic_enum::enum_name(restore_err) << "\n";
		return 1;
	}

	const auto iteration_count = 10000;
	auto       exec_durations = std::vector<std::chrono::nanoseconds>{};
	exec_durations.resize(iteration_count);

	auto progress_message = benchmark_progress_message{};
	auto result_message = benchmark_result_message{};

	for(auto i = 0; iteration_count > i; ++i) {
		auto before = clock_t::now();
		exec_systems_fn(reg_id, 1, nullptr, nullptr);
		auto after = clock_t::now();

		auto exec_duration = duration_cast<nanoseconds>(after - before);

		exec_durations[i] = exec_duration;

		result_message.total_duration_ms +=
			duration_cast<duration<float, std::milli>>(exec_duration).count();
		if(i % 100 == 0) {
			progress_message.progress =
				static_cast<float>(i) / static_cast<float>(iteration_count);
			reporter.report(progress_message);
		}
	}

	result_message.average_duration_ms =
		result_message.total_duration_ms / static_cast<float>(iteration_count);

	progress_message.progress = 1.0f;
	reporter.report(progress_message);
	reporter.report(result_message);

	return 0;
}
