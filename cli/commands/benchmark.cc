#include "./benchmark.hh"

#include <iostream>
#include <string>
#include <cassert>
#include <cstdio>
#include <filesystem>
#include <chrono>
#include <ranges>
#include <variant>
#include <thread>
#include <ranges>
#include <boost/dll/shared_library.hpp>
#include <boost/dll/library_info.hpp>
#include "docopt.h"
#include "nlohmann/json.hpp"
#include "ecsact/runtime/core.h"
#include "ecsact/runtime/core.hh"
#include "ecsact/runtime/serialize.h"
#include "ecsact/runtime/async.h"
#include "ecsactsi_wasm.h"
#include "magic_enum.hpp"

using std::chrono::duration;
using std::chrono::duration_cast;
using std::chrono::nanoseconds;

namespace fs = std::filesystem;
using benchmark_clock_t = std::chrono::high_resolution_clock;

constexpr auto USAGE = R"(Ecsact Benchmark Command

Usage:
	ecsact benchmark (-h | --help)
	ecsact benchmark <system_impl>... --runtime=<path> --seed=<path>
		[--async=<connect_string>] [--events=summary]
		[--iterations=<count>] [--iteration_report_interval=<count>]
)";

constexpr auto OPTIONS = R"(
Options:
	<system_impl>
		Path to Ecsact system implementation binaries. If the meta module is not
		available on your runtime binary you must specify the system export name
		and system ID in this format: `path;export-name,id`. Multiple exports
		may be added in semi-colon (;) deliminated list.
		NOTE: Only WebAssembly is allowed at this time.
	--runtime=<path>
		Path to built Ecsact Runtime typically one from ecsact_rtb.
	--seed=<path>
		Path to file containing entity seed data from an ecsact_dump_entities
		call. The format must be compatible with the runtime because
		ecsact_restore_entities will be called with said data.
	--async=<connect_string>
		Connect to an async runtime via <connect_string> instead of executing.
	--events=summary
		End of benchmark will give a report of how many of each event occured
		during the benchmark.
	--iterations=<count>  [default: 10000]
		Number of times ecsact_execute_systems is called or in the case of async
		number of ticks that pass until disconnect.
	--iteration_report_interval=<count>  [default: 100]
		How often an iteration progress is reported.
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

struct component_event_report_item {
	ecsact_event        event = {};
	ecsact_component_id component_id = {};
	int                 count = 0;

	NLOHMANN_DEFINE_TYPE_INTRUSIVE(
		component_event_report_item,
		event,
		component_id,
		count
	);
};

struct entity_event_report_item {
	ecsact_event event = {};
	int          count = 0;

	NLOHMANN_DEFINE_TYPE_INTRUSIVE(entity_event_report_item, event, count);
};

struct event_summary_report_message {
	static constexpr auto type = "event_summary";

	std::vector<component_event_report_item> component_events;
	std::vector<entity_event_report_item>    entity_events;

	inline auto component_report(ecsact_event event, ecsact_component_id comp_id)
		-> component_event_report_item& {
		for(auto& entry : component_events) {
			if(entry.event == event && entry.component_id == comp_id) {
				return entry;
			}
		}

		auto& result = component_events.emplace_back();
		result.component_id = comp_id;
		result.event = event;

		return result;
	}

	inline auto entity_report(ecsact_event event) -> entity_event_report_item& {
		for(auto& entry : entity_events) {
			if(entry.event == event) {
				return entry;
			}
		}

		auto& result = entity_events.emplace_back();
		result.event = event;

		return result;
	}

	NLOHMANN_DEFINE_TYPE_INTRUSIVE(
		event_summary_report_message,
		component_events,
		entity_events
	);
};

using benchmark_message_variant_t = std::variant<
	info_message,
	warning_message,
	error_message,
	benchmark_progress_message,
	benchmark_result_message,
	event_summary_report_message>;

class stdout_json_benchmark_reporter {
	template<typename MessageT>
	void _report(MessageT& message) {
		auto message_json = "{}"_json;
		to_json(message_json, message);
		message_json["type"] = MessageT::type;
		std::cout << message_json.dump() + "\n";
		std::cout.flush();
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

auto expect_docopt_value_long(
	const auto&       args,
	const std::string arg_name,
	long              default_value
) -> long {
	const docopt::value& arg = args.at(arg_name);

	if(!arg) {
		return default_value;
	}

	try {
		return arg.asLong();
	} catch(const std::invalid_argument&) {
		std::cerr //
			<< "[ERROR] Expected integer for: " << arg_name << " instead got " << arg
			<< "\n"
			<< "For details run:\tecsact benchmark --help\n";
		std::exit(1);
	}
}

auto report_component_event_summary(
	ecsact_event        event,
	ecsact_entity_id    _entity_id,
	ecsact_component_id component_id,
	const void*         _component_data,
	void*               user_data
) -> void {
	auto summary_report = static_cast<event_summary_report_message*>(user_data);
	summary_report->component_report(event, component_id).count += 1;
}

auto report_entity_event_summary(
	ecsact_event                 event,
	ecsact_entity_id             _entity_id,
	ecsact_placeholder_entity_id _placeholder_entity_id,
	void*                        user_data
) -> void {
	auto summary_report = static_cast<event_summary_report_message*>(user_data);
	summary_report->entity_report(event).count += 1;
}

struct common_benchmark_options {
	boost::dll::shared_library&        runtime;
	stdout_json_benchmark_reporter&    reporter;
	ecsact_execution_events_collector& evc;
	FILE*                              seed_file;
	long                               iterations;
	long                               iteration_report_interval;
};

auto start_async_benchmark(
	std::string                     connect_string,
	const common_benchmark_options& options
) -> std::optional<benchmark_result_message> {
	using namespace std::string_literals;
	using namespace std::chrono_literals;

	auto result_message = benchmark_result_message{};

	const auto async_connect_fn = get_or_exit<decltype(ecsact_async_connect)>(
		options.runtime,
		"ecsact_async_connect"
	);
	const auto async_disconnect_fn =
		get_or_exit<decltype(ecsact_async_disconnect)>(
			options.runtime,
			"ecsact_async_disconnect"
		);
	const auto async_flush_fn = get_or_exit<decltype(ecsact_async_flush_events)>(
		options.runtime,
		"ecsact_async_flush_events"
	);
	const auto async_enqueue_exec_options_fn =
		get_or_exit<decltype(ecsact_async_enqueue_execution_options)>(
			options.runtime,
			"ecsact_async_enqueue_execution_options"
		);
	const auto async_get_current_tick =
		get_or_exit<decltype(ecsact_async_get_current_tick)>(
			options.runtime,
			"ecsact_async_get_current_tick"
		);

	const auto restore_as_exec_options_fn =
		get_or_exit<decltype(ecsact_restore_as_execution_options)>(
			options.runtime,
			"ecsact_restore_as_execution_options"
		);

	options.reporter.report(info_message{"Async Connect: " + connect_string});

	struct {
		ecsact_async_request_id                 connect_req_id;
		decltype(options.reporter)&             reporter;
		bool                                    done;
		bool                                    connected;
		decltype(async_enqueue_exec_options_fn) async_enqueue_exec_options_fn;
		ecsact_async_request_id                 restore_enqueue_req_id;
	} vars{
		.connect_req_id = async_connect_fn(connect_string.c_str()),
		.reporter = options.reporter,
		.done = false,
		.connected = false,
		.async_enqueue_exec_options_fn = async_enqueue_exec_options_fn,
		.restore_enqueue_req_id = {},
	};

	auto async_evc = ecsact_async_events_collector{};
	async_evc.system_error_callback_user_data = &vars;
	async_evc.system_error_callback = //
		[](ecsact_execute_systems_error err, void* user_data) {
			auto vars_ptr = static_cast<decltype(&vars)>(user_data);
			vars_ptr->reporter.report(error_message{
				"System Execution Error: " + std::string(magic_enum::enum_name(err)),
			});
		};

	async_evc.async_error_callback_user_data = &vars;
	async_evc.async_error_callback = //
		[](
			ecsact_async_error       err,
			int32_t                  req_ids_count,
			ecsact_async_request_id* req_ids_raw,
			void*                    user_data
		) {
			auto vars_ptr = static_cast<decltype(&vars)>(user_data);
			auto req_ids = std::span{req_ids_raw, static_cast<size_t>(req_ids_count)};

			for(auto& req_id : req_ids) {
				if(req_id == vars_ptr->connect_req_id) {
					vars_ptr->reporter.report(error_message{
						"Async connect failed: "s + std::string(magic_enum::enum_name(err)),
					});
				} else {
					vars_ptr->reporter.report(error_message{
						"Async error (req="s + std::to_string(static_cast<int>(req_id)) +
							"): "s + std::string(magic_enum::enum_name(err)),
					});
				}
			}

			vars_ptr->done = true;
		};
	async_evc.async_request_done_callback_user_data = &vars;
	async_evc.async_request_done_callback = //
		[](
			int32_t                  req_ids_count,
			ecsact_async_request_id* req_ids_raw,
			void*                    user_data
		) {
			auto vars_ptr = static_cast<decltype(&vars)>(user_data);
			auto req_ids = std::span{req_ids_raw, static_cast<size_t>(req_ids_count)};

			for(auto req_id : req_ids) {
				if(req_id == vars_ptr->connect_req_id) {
					vars_ptr->connected = true;
					vars_ptr->reporter.report(info_message{
						"Async successfully connected",
					});
				}
			}
		};

	while(!vars.connected) {
		std::this_thread::yield();
		async_flush_fn(&options.evc, &async_evc);
	}

	if(vars.done) {
		return {};
	}

	auto restore_err = restore_as_exec_options_fn(
		[](void* out_data, int32_t data_max_length, void* ud) -> int32_t {
			return std::fread(out_data, 1, data_max_length, static_cast<FILE*>(ud));
		},
		options.seed_file,
		[](ecsact_execution_options exec_options, void* ud) {
			auto vars_ptr = static_cast<decltype(&vars)>(ud);

			vars_ptr->restore_enqueue_req_id =
				vars_ptr->async_enqueue_exec_options_fn(exec_options);
		},
		&vars
	);

	if(restore_err != ECSACT_RESTORE_OK) {
		std::cerr //
			<< "Seed entities failed to restore: "
			<< magic_enum::enum_name(restore_err) << "\n";
		return {};
	}

	auto progress_message = benchmark_progress_message{};
	auto async_start = benchmark_clock_t::now();

	while(!vars.done) {
		std::this_thread::yield();

		async_flush_fn(&options.evc, &async_evc);
		auto tick = async_get_current_tick();

		if(tick % options.iteration_report_interval == 0) {
			progress_message.progress =
				static_cast<float>(tick) / static_cast<float>(options.iterations);
			options.reporter.report(progress_message);
		}

		if(tick >= options.iterations) {
			options.reporter.report(info_message{
				"Async benchmark ended at tick " + std::to_string(tick),
			});
			break;
		}
	}

	auto total_duration = duration_cast<duration<float, std::milli>>(
		benchmark_clock_t::now() - async_start
	);

	result_message.total_duration_ms = total_duration.count();

	async_disconnect_fn();

	return result_message;
}

auto start_core_benchmark(const common_benchmark_options& options)
	-> std::optional<benchmark_result_message> {
	auto result_message = benchmark_result_message{};

	const auto create_reg_fn = get_or_exit<decltype(ecsact_create_registry)>(
		options.runtime,
		"ecsact_create_registry"
	);
	const auto exec_systems_fn = get_or_exit<decltype(ecsact_execute_systems)>(
		options.runtime,
		"ecsact_execute_systems"
	);
	const auto restore_fn = get_or_exit<decltype(ecsact_restore_entities)>(
		options.runtime,
		"ecsact_restore_entities"
	);

	auto reg_id = create_reg_fn("BenchmarkRegistry");

	auto restore_err = restore_fn(
		reg_id,
		[](void* out_data, int32_t data_max_length, void* ud) -> int32_t {
			return std::fread(out_data, 1, data_max_length, static_cast<FILE*>(ud));
		},
		nullptr,
		options.seed_file
	);

	if(restore_err != ECSACT_RESTORE_OK) {
		std::cerr //
			<< "Seed entities failed to restore: "
			<< magic_enum::enum_name(restore_err) << "\n";
		return {};
	}

	auto progress_message = benchmark_progress_message{};
	auto exec_durations = std::vector<std::chrono::nanoseconds>{};
	exec_durations.resize(options.iterations);

	for(auto i = 0; options.iterations > i; ++i) {
		auto before = benchmark_clock_t::now();
		exec_systems_fn(reg_id, 1, nullptr, &options.evc);
		auto after = benchmark_clock_t::now();

		auto exec_duration = duration_cast<nanoseconds>(after - before);

		exec_durations[i] = exec_duration;

		result_message.total_duration_ms +=
			duration_cast<duration<float, std::milli>>(exec_duration).count();
		if(i % options.iteration_report_interval == 0) {
			progress_message.progress =
				static_cast<float>(i) / static_cast<float>(options.iterations);
			options.reporter.report(progress_message);
		}
	}

	return result_message;
}

int ecsact::cli::detail::benchmark_command(int argc, char* argv[]) {
	using namespace std::string_literals;
	using namespace std::chrono_literals;

	auto args = docopt::docopt(USAGE, {argv + 1, argv + argc}, false);

	if(args["--help"] && args["--help"].asBool()) {
		std::cout << USAGE << OPTIONS;
		return 0;
	}

	auto async = //
		args["--async"] ? std::optional(args["--async"].asString()) : std::nullopt;
	auto iterations = expect_docopt_value_long(args, "--iterations", 10000L);
	auto iteration_report_interval =
		expect_docopt_value_long(args, "--iteration_report_interval", 100L);
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

	auto evc = ecsact_execution_events_collector{};
	auto event_summary = std::optional<event_summary_report_message>{};

	if(args["--events"]) {
		auto event_summary_ptr = &event_summary.emplace();
		evc.init_callback = &report_component_event_summary;
		evc.init_callback_user_data = event_summary_ptr;

		evc.update_callback = &report_component_event_summary;
		evc.update_callback_user_data = event_summary_ptr;

		evc.remove_callback = &report_component_event_summary;
		evc.remove_callback_user_data = event_summary_ptr;

		evc.entity_created_callback = &report_entity_event_summary;
		evc.entity_created_callback_user_data = event_summary_ptr;

		evc.entity_destroyed_callback = &report_entity_event_summary;
		evc.entity_destroyed_callback_user_data = event_summary_ptr;
	}

	auto seed_file = std::fopen(seed_path.c_str(), "r");
	if(seed_file == nullptr) {
		std::cerr << "Failed to open seed path: " << seed_path << "\n";
		return 1;
	}

	auto benchmark_options = common_benchmark_options{
		.runtime = runtime,
		.reporter = reporter,
		.evc = evc,
		.seed_file = seed_file,
		.iterations = iterations,
		.iteration_report_interval = iteration_report_interval,
	};

	auto result_message = std::optional<benchmark_result_message>{};

	if(async) {
		result_message = start_async_benchmark(async.value(), benchmark_options);
	} else {
		result_message = start_core_benchmark(benchmark_options);
	}

	if(event_summary.has_value()) {
		reporter.report(event_summary.value());
	}

	if(result_message) {
		auto& result_message_val = result_message.value();
		result_message_val.average_duration_ms =
			result_message_val.total_duration_ms / static_cast<float>(iterations);
		reporter.report(result_message_val);
	}

	return 0;
}
