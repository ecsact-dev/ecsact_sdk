#include "./codegen.hh"

#include <iostream>
#include <fstream>
#include <vector>
#include <filesystem>
#include <string_view>
#include <unordered_set>
#include <boost/dll/shared_library.hpp>
#include "docopt.h"
#include "ecsact/parse_runtime_interop.h"
#include "ecsact/runtime/meta.h"

#include "executable_path/executable_path.hh"

namespace fs = std::filesystem;

constexpr auto USAGE = R"(Ecsact Codegen Command

Usage:
	ecsact codegen <files> --plugin=<plugin> [--stdout]
	ecsact codegen <files>... --plugin=<plugin>...

Options:
	-p, --plugin=<plugin>
		Name of bundled plugin or path to plugin.
	--stdout
		Print to stdout instead of writing to a file. May only be used if a single
		esact file and single ecsact codegen plugin are used.
)";

static fs::path get_default_plugins_dir() {
	using executable_path::executable_path;

	return fs::weakly_canonical(
		executable_path() / ".." / "share" / "ecsact" / "plugins"
	);
}

static auto platform_plugin_extension() {
	return boost::dll::shared_library::suffix().string();
}

static std::optional<fs::path> resolve_plugin_path
	( const std::string&      plugin_arg
	, const fs::path&         default_plugins_dir
	, std::vector<fs::path>&  checked_plugin_paths
	)
{
	using namespace std::string_literals;

	const bool is_maybe_named_plugin =
		plugin_arg.find('/') == std::string::npos &&
		plugin_arg.find('\\') == std::string::npos &&
		plugin_arg.find('.') == std::string::npos;

	if(is_maybe_named_plugin) {
		fs::path& default_plugin_path = checked_plugin_paths.emplace_back(
			default_plugins_dir / ("ecsact_"s + plugin_arg + "_codegen"s)
		);

		default_plugin_path.replace_extension(platform_plugin_extension());

		if(fs::exists(default_plugin_path)) {
			return default_plugin_path;
		}
	}

	fs::path plugin_path = fs::weakly_canonical(plugin_arg);
	if(plugin_path.extension().empty()) {
		plugin_path.replace_extension(platform_plugin_extension());
	}

	if(fs::exists(plugin_path)) {
		return plugin_path;
	}

	checked_plugin_paths.emplace_back(plugin_path);

	return {};
}

static void stdout_write_fn
	( const char*  str
	, int32_t      str_len
	)
{
	std::cout << std::string_view(str, str_len);
}

static thread_local std::ofstream file_write_stream;
static void file_write_fn
	( const char*  str
	, int32_t      str_len
	)
{
	file_write_stream << std::string_view(str, str_len);
}

int ecsact::cli::detail::codegen_command(int argc, char* argv[]) {
	using namespace std::string_literals;

	auto args = docopt::docopt(USAGE, {argv + 1, argv + argc});
	bool files_error = false;
	auto files = args.at("<files>").asStringList();

	for(fs::path file_path : files) {
		if(file_path.extension() != ".ecsact") {
			files_error = true;
			std::cerr
				<< "[ERROR] Expected .ecsact file instead got'"
				<< file_path.string()
				<< "'\n";
		} else if(!fs::exists(file_path)) {
			files_error = true;
			std::cerr << "[ERROR] '" << file_path.string() << "' does not exist\n";
		}
	}

	auto default_plugins_dir = get_default_plugins_dir();
	std::vector<fs::path> plugin_paths;
	std::vector<boost::dll::shared_library> plugins;
	bool plugins_not_found = false;

	for(auto plugin_arg : args.at("--plugin").asStringList()) {
		std::vector<fs::path> checked_plugin_paths;
		auto plugin_path = resolve_plugin_path(
			plugin_arg,
			default_plugins_dir,
			checked_plugin_paths
		);

		if(plugin_path) {
			plugin_paths.emplace_back(*plugin_path);
		} else {
			plugins_not_found = true;
			std::cerr
				<< "[ERROR] Unable to find codegen plugin '" << plugin_arg
				<< "'. Paths checked:\n";

			for(auto& checked_path : checked_plugin_paths) {
				std::cerr << " - " << fs::absolute(checked_path).string() << "\n";
			}
		}
	}

	bool plugin_load_failed = false;
	plugins.reserve(plugin_paths.size());

	for(auto plugin_path : plugin_paths) {
		boost::system::error_code ec;
		auto& plugin = plugins.emplace_back();
		plugin.load(plugin_path.string(), ec);
		if(ec) {
			plugin_load_failed = true;
			std::cerr
				<< "[ERROR] Failed to load codegen plugin '"
				<< plugin_path.filename().string() << "': "
				<< ec.message()
				<< " (" << ec.value() << ")"
				<< "\n";
		}
	}

	if(plugin_load_failed || plugins_not_found || files_error) {
		return 1;
	}

	std::vector<const char*> cstr_files;
	cstr_files.reserve(files.size());
	for(auto& file : files) {
		cstr_files.emplace_back() = file.c_str();
	}

	ecsact_parse_runtime_interop(
		cstr_files.data(),
		static_cast<int32_t>(cstr_files.size())
	);

	std::vector<ecsact_package_id> package_ids;
	package_ids.resize(static_cast<int32_t>(ecsact_meta_count_packages()));
	ecsact_meta_get_package_ids(
		static_cast<int32_t>(package_ids.size()),
		package_ids.data(),
		nullptr
	);

	std::unordered_set<std::string> output_paths;
	bool has_conflicting_output_paths = false;

	for(auto& plugin : plugins) {
		using write_fn_t = void(*)(const char*, int32_t);
		auto& write_fn_ptr = plugin.get<write_fn_t&>("ecsact_codegen_write");
		auto& plugin_fn = plugin.get<void(int32_t)>("ecsact_codegen_plugin");
		auto plugin_name =
			plugin.get<const char*()>("ecsact_codegen_plugin_name")();

		write_fn_ptr = &file_write_fn;
		for(auto package_id : package_ids) {
			fs::path output_file_path = ecsact_meta_package_file_path(package_id);
			output_file_path.replace_extension(
				output_file_path.extension().string() + "." + plugin_name
			);
			if(file_write_stream.is_open()) {
				file_write_stream.close();
			}
			if(output_paths.contains(output_file_path.string())) {
				has_conflicting_output_paths = true;
				std::cerr
					<< "[ERROR] Plugin '"
					<< plugin.location().filename()
					<< "' has conflicts with another plugin output file '"
					<< output_file_path.string() << "'\n";
				continue;
			}

			output_paths.emplace(output_file_path.string());
			file_write_stream.open(output_file_path);
			plugin_fn(static_cast<int32_t>(package_id));
		}
		write_fn_ptr = nullptr;
	}

	if(has_conflicting_output_paths) {
		return 2;
	}

	return 0;
}
