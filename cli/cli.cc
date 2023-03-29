#include <iostream>
#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_map>
#include "cli/bazel_stamp_header.hh"

#include "./commands/command.hh"
#include "./commands/codegen.hh"
#include "./commands/config.hh"

using namespace std::string_view_literals;

constexpr auto LOGO_RAW = R"(
          ########\                                          ##\     
          ##  _____|                                         ## |    
          ## |      #######\  #######\  ######\   #######\ ######\   
          #####\   ##  _____|##  _____| \____##\ ##  _____|\_##  _|  
          ##  __|  ## /      \######\   ####### |## /        ## |    
          ## |     ## |       \____##\ ##  __## |## |        ## |##\ 
          ########\\#######\ #######  |\####### |\#######\   \####  |
          \________|\_______|\_______/  \_______| \_______|   \____/ 
)"sv;

constexpr auto USAGE = R"(Ecsact SDK Command Line

Usage:
	ecsact (--help | -h)
	ecsact (--version | -v)
	ecsact config ([<options>...] | --help)
	ecsact codegen ([<options>...] | --help)
)";

std::string colorize_logo() {
	std::string logo;
	logo.reserve(LOGO_RAW.size() * 2);

	std::string curr_color;
	for(auto c : LOGO_RAW) {
		std::string color;
		if(c == '\\' || c == '|' || c == '_' || c == '/') {
			color = "\x1B[90m";
		} else if(c == '#') {
			color = "\x1B[36m";
		}

		if(!color.empty() && color != curr_color) {
			logo += color;
			curr_color = color;
		}

		logo += c;
	}

	logo += "\x1B[0m";

	return logo;
}

void print_usage() {
	std::cerr << colorize_logo() << "\n" << USAGE;
}

int main(int argc, char* argv[]) {
	using ecsact::cli::detail::command_fn_t;

	const std::unordered_map<std::string, command_fn_t> commands{
		{"codegen", &ecsact::cli::detail::codegen_command},
		{"config", &ecsact::cli::detail::config_command},
	};

	if(argc >= 2) {
		std::string command = argv[1];

		if(command == "-h" || command == "--help") {
			print_usage();
			return 0;
		} else if(command == "-v" || command == "--version") {
			std::cout << STABLE_ECSACT_SDK_VERSION << "\n";
			return 0;
		} else if(command.starts_with('-')) {
			std::cerr << "Expected subcommand and instead got '" << command << "'\n";
			print_usage();
			return 1;
		}

		if(!commands.contains(command)) {
			std::cerr << "Invalid subcommand: " << command << "\n";
			print_usage();
			return 2;
		}

		return commands.at(command)(argc, argv);
	}

	std::cerr << "Expected subcommand.\n";
	print_usage();
	return 1;
}
