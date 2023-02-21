#include "./config.hh"

#include <iostream>
#include <filesystem>
#include <map>
#include <string>
#include "docopt.h"
#include "nlohmann/json.hpp"

#include "executable_path/executable_path.hh"

namespace fs = std::filesystem;

constexpr auto USAGE = R"(Ecsact Config Command

Usage:
	ecsact config [<keys>...]

Options:
	<keys>
		Configuration keys. When 1 key is provided the config value for that key is 
		printed to stdout. If no keys or more than 1 key is provided then a JSON 
		object is printed to stdout.
		
		Available config keys:
			install_dir       directory Ecsact SDK was installed to
			include_dir       directory containing Ecsact headers
			plugin_dir        directory containing built-in Ecsact codegen plugins
			builtin_plugins   list of built-in Ecsact codegen plugins available.

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
	auto output = "{}"_json;

	std::unordered_map<std::string, std::function<int()>> key_handlers{
		{
			"install_dir",
			[&] {
				output["install_dir"] = install_prefix.string();
				return 0;
			},
		},
		{
			"include_dir",
			[&] {
				auto includedir = install_prefix / "include";
				auto core_hdr = includedir / "ecsact" / "runtime" / "core.h";

				if(fs::exists(core_hdr)) {
					output["include_dir"] = includedir.string();
				} else {
					std::cerr << CANNOT_FIND_INCLUDE_DIR;
					return 1;
				}

				return 0;
			},
		},
		{
			"plugin_dir",
			[&] {
				if(fs::exists(plugin_dir)) {
					output["plugin_dir"] = plugin_dir.string();
				} else {
					std::cerr << CANNOT_FIND_PLUGIN_DIR;
					return 1;
				}

				return 0;
			},
		},
		{
			"builtin_plugins",
			[&] {
				if(fs::exists(plugin_dir)) {
					std::vector<std::string> builtin_plugins;
					for(auto& entry : fs::directory_iterator(plugin_dir)) {
						auto filename =
							entry.path().filename().replace_extension("").string();
						const auto prefix = "ecsact_"s;
						const auto suffix = "_codegen"s;
						if(filename.starts_with(prefix) && filename.ends_with(suffix)) {
							builtin_plugins.emplace_back() = filename.substr(
								prefix.size(),
								filename.size() - prefix.size() - suffix.size()
							);
						}
					}

					output["builtin_plugins"] = builtin_plugins;
				} else {
					std::cerr << CANNOT_FIND_PLUGIN_DIR;
					return 1;
				}

				return 0;
			},
		},
	};

	auto keys = args.at("<keys>").asStringList();
	if(keys.empty()) {
		for(auto&& [_, key_handler] : key_handlers) {
			int exit_code = key_handler();
			if(exit_code != 0) {
				return exit_code;
			}
		}
	} else {
		for(auto key : keys) {
			if(!key_handlers.contains(key)) {
				std::cerr << "[ERROR] Invalid config key: " << key << "\n\n";
				std::cerr << USAGE;
				return 1;
			}

			int exit_code = key_handlers.at(key)();
			if(exit_code != 0) {
				return exit_code;
			}
		}
	}

	if(output.size() == 1) {
		auto& value = output.begin().value();
		if(value.is_array()) {
			for(auto& element : value) {
				std::cout << element.get<std::string>() << "\n";
			}
		} else if(value.is_string()) {
			std::cout << value.get<std::string>();
		} else {
			std::cout << value;
		}
	} else {
		std::cout << output.dump();
	}

	return 0;
}
