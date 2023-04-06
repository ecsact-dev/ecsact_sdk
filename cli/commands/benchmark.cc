#include "./benchmark.hh"

#include <iostream>
#include <string>
#include <cassert>
#include <cstdio>
#include <filesystem>
#include <chrono>
#include <boost/dll/shared_library.hpp>
#include <boost/dll/library_info.hpp>
#include "docopt.h"
#include "ecsact/runtime/core.h"
#include "ecsact/runtime/serialize.h"
#include "magic_enum.hpp"

namespace fs = std::filesystem;

constexpr auto USAGE = R"(Ecsact Benchmark Command

Usage:
	ecsact benchmark --runtime=<path> --seed=<path>

Options:
	--runtime=<path>
		Path to built Ecsact Runtime typically one from ecsact_rtb.
	--seed=<path>
		Path to file containing entity seed data from an ecsact_dump_entities
		call. The format must be compatible with the runtime because
		ecsact_restore_entities will be called with said data.
)";

template<typename T>
static auto get_or_exit(boost::dll::shared_library& runtime) -> T& {
	// for C functions this should be fine
	auto fn_name = std::string(typeid(T).name());
	assert(fn_name.starts_with("ecsact_"));

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

int ecsact::cli::detail::benchmark_command(int argc, char* argv[]) {
	using clock_t = std::chrono::high_resolution_clock;

	auto args = docopt::docopt(USAGE, {argv + 1, argv + argc});
	auto runtime_path = args["--runtime"].asString();
	auto seed_path = args["--seed"].asString();
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

	const auto create_reg_fn =
		get_or_exit<decltype(ecsact_create_registry)>(runtime);
	const auto exec_systems_fn =
		get_or_exit<decltype(ecsact_execute_systems)>(runtime);
	const auto restore_fn =
		get_or_exit<decltype(ecsact_restore_entities)>(runtime);

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
		&seed_file
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

	return 0;
}
