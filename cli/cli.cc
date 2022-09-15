#include <iostream>
#include <filesystem>
#include <string>
#include <unordered_map>

#include "./commands/command.hh"
#include "./commands/codegen.hh"
#include "./commands/config.hh"

namespace fs = std::filesystem;

constexpr auto USAGE = R"(Ecsact SDK Command Line

Usage:
	ecsact [options] <command> [<args>]
)";

int main(int argc, char* argv[]) {
	using ecsact::cli::detail::command_fn_t;

	if(argc <= 1) {
		std::cerr << USAGE;
		return 1;
	}

	const std::unordered_map<std::string, command_fn_t> commands{
		{"codegen", &ecsact::cli::detail::codegen_command},
		{"config", &ecsact::cli::detail::config_command},
	};

	for(int i=1; argc > i; ++i) {
		if(argv[i][0] == '-') continue;

		std::string arg = argv[i];
		if(!commands.contains(arg)) continue;

		return commands.at(arg)(argc, argv);
	}

	std::cerr << USAGE;

	return 1;
}
