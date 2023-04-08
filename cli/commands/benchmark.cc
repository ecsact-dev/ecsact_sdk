#include "./benchmark.hh"

#include <iostream>
#include <string>
#include <cassert>
#include <cstdio>
#include <filesystem>
#include <chrono>
#include <ranges>
#include <boost/dll/shared_library.hpp>
#include <boost/dll/library_info.hpp>
#include "docopt.h"
#include "ecsact/runtime/core.h"
#include "ecsact/runtime/serialize.h"
#include "ecsactsi_wasm.h"
#include "magic_enum.hpp"

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
					str.substr(semi_colon_index, comma_index - semi_colon_index);

				auto& system_id = parsed_arg.system_ids.emplace_back();
				system_id = static_cast<ecsact_system_like_id>(std::atoi(
					str.substr(comma_index, next_semi_colon_index - comma_index).c_str()
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

		for(auto export_name : export_names) {
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
				<< "Failed to load Wasm File: " << magic_enum::enum_name(err)
				<< "\n  path: " << system_impl_binary_path_str << "\n";
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

	for(auto i = 0; iteration_count > i; ++i) {
		auto before = clock_t::now();
		exec_systems_fn(reg_id, 1, nullptr, nullptr);
		auto after = clock_t::now();

		auto exec_duration =
			std::chrono::duration_cast<std::chrono::nanoseconds>(before - after);

		exec_durations[i] = exec_duration;
	}

	std::cout << "Done!\n";

	return 0;
}
