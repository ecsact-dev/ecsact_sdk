#include "./config.hh"

#include <iostream>
#include <filesystem>
#include <map>
#include <string>
#include "docopt.h"

#include "executable_path/executable_path.hh"

namespace fs = std::filesystem;

constexpr auto USAGE = R"(Ecsact Config Command

Usage:
	ecsact config [--include_dir] [--plugin_dir] [--builtin_plugins]

Options:
	--include_dir
		Prints the include directory containing Ecsact headers.
	--plugin_dir
		Prints the directory containing all built-in Ecsact codegen plugins.
	--builtin_plugins
		Prints a list of built-in Ecsact codegen plugins available.
)";

constexpr auto CANNOT_FIND_INCLUDE_DIR = R"(
[ERROR] Cannot find Ecsact include directory.
	Expected include directory next to bin directory. Make sure you're using a
	standard Ecsact SDK installation.
	If you believe this is a mistake please file an issue at
	https://github.com/ecsact-dev/ecsact_sdk/issues
)";

constexpr auto CANNOT_FIND_PLUGIN_DIR = R"(
[ERROR] Cannot find Ecsact built-in plugin directory.
	Make sure you're using a standard Ecsact SDK installation.
	If you believe this is a mistake please file an issue at
	https://github.com/ecsact-dev/ecsact_sdk/issues
)";

int ecsact::cli::detail::config_command(int argc, char* argv[]) {
	using namespace std::string_literals;
	using executable_path::executable_path;

	auto args = docopt::docopt(USAGE, {argv + 1, argv + argc});
	auto exec_path = executable_path();
	if(exec_path.empty()) {
		exec_path = fs::path{argv[0]};
		std::error_code ec;
		exec_path = fs::canonical(exec_path, ec);
		if(ec) {
			exec_path = fs::weakly_canonical(fs::path{argv[0]});
		}
	}

	auto install_prefix = exec_path.parent_path().parent_path();
	auto plugin_dir = install_prefix / "share" / "ecsact" / "plugins";
	std::map<std::string, std::string> output;

	if(args.at("--include_dir").asBool()) {
		auto includedir = install_prefix / "include";
		auto core_hdr = includedir / "ecsact" / "runtime" / "core.h";

		if(fs::exists(core_hdr)) {
			output["include_dir"] = includedir.string();
		} else {
			std::cerr << CANNOT_FIND_INCLUDE_DIR;
			return 1;
		}
	}

	if(args.at("--plugin_dir").asBool()) {
		if(fs::exists(plugin_dir)) {
			output["plugin_dir"] = plugin_dir.string();
		} else {
			std::cerr << CANNOT_FIND_PLUGIN_DIR;
			return 1;
		}
	}

	if(args.at("--builtin_plugins").asBool()) {
		if(fs::exists(plugin_dir)) {
			auto& builtin_plugins_str = output["builtin_plugins"];
			std::vector<std::string> builtin_plugins;
			for(auto& entry : fs::directory_iterator(plugin_dir)) {
				auto filename = entry.path().filename().replace_extension("").string();
				const auto prefix = "ecsact_"s;
				const auto suffix = "_codegen"s;
				if(filename.starts_with(prefix) && filename.ends_with(suffix)) {
					builtin_plugins.push_back(
						filename.substr(
							prefix.size(),
							filename.size() - prefix.size() - suffix.size()
						)
					);
				}
			}

			if(!builtin_plugins.empty()) {
				auto& builtin_plugins_str = output["builtin_plugins"];
				for(auto i=0; builtin_plugins.size() - 1 > i; ++i) {
					builtin_plugins_str += builtin_plugins[i] + "\n";
				}
				builtin_plugins_str += builtin_plugins.back();
			}
		} else {
			std::cerr << CANNOT_FIND_PLUGIN_DIR;
			return 1;
		}
	}

	if(output.empty()) {
		return 1;
	}

	if(output.size() == 1) {
		std::cout << output.begin()->second << "\n";
	} else {
		for(auto&& [key, value] : output) {
			std::cout << key << ": " << value << "\n";
		}
	}

	return 0;
}
