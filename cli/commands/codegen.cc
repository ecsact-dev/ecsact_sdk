#include "./codegen.hh"

#include <iostream>
#include <fstream>
#include <vector>
#include <filesystem>
#include <string_view>
#include <unordered_set>
#include <boost/dll/shared_library.hpp>
#include <boost/dll/library_info.hpp>
#include "docopt.h"
#include "ecsact/interpret/eval.hh"
#include "ecsact/runtime/meta.h"
#include "ecsact/runtime/dylib.h"
#include "ecsact/codegen_plugin.h"
#include "ecsact/codegen/plugin_validate.hh"
#include "magic_enum.hpp"

#include "executable_path/executable_path.hh"

namespace fs = std::filesystem;
constexpr auto file_readonly_perms = fs::perms::others_read |
	fs::perms::group_read | fs::perms::owner_read;

constexpr auto USAGE = R"(Ecsact Codegen Command

Usage:
	ecsact codegen <files>... --plugin=<plugin> [--stdout]
	ecsact codegen <files>... --plugin=<plugin>... [--outdir=<directory>]

Options:
	-p, --plugin=<plugin>
		Name of bundled plugin or path to plugin.
	--stdout
		Print to stdout instead of writing to a file. May only be used if a single
		ecsact file and single ecsact codegen plugin are used.
	-o, --outdir=<directory>
		Specify directory generated files should be written to. By default generated
		files are written next to source files.
)";

static fs::path get_default_plugins_dir() {
	using executable_path::executable_path;

	return fs::weakly_canonical(
		executable_path().parent_path() / ".." / "share" / "ecsact" / "plugins"
	);
}

static auto platform_plugin_extension() {
	return boost::dll::shared_library::suffix().string();
}

static std::optional<fs::path> resolve_plugin_path(
	const std::string&     plugin_arg,
	const fs::path&        default_plugins_dir,
	std::vector<fs::path>& checked_plugin_paths
) {
	using namespace std::string_literals;

	const bool is_maybe_named_plugin = plugin_arg.find('/') ==
			std::string::npos &&
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

static void stdout_write_fn(const char* str, int32_t str_len) {
	std::cout << std::string_view(str, str_len);
}

static thread_local std::ofstream file_write_stream;

static void file_write_fn(const char* str, int32_t str_len) {
	file_write_stream << std::string_view(str, str_len);
}

int ecsact::cli::detail::codegen_command(int argc, char* argv[]) {
	using namespace std::string_literals;

	auto                  args = docopt::docopt(USAGE, {argv + 1, argv + argc});
	bool                  files_error = false;
	auto                  files_str = args.at("<files>").asStringList();
	std::vector<fs::path> files;
	files.reserve(files_str.size());

	for(fs::path file_path : files_str) {
		files.push_back(file_path);
		if(file_path.extension() != ".ecsact") {
			files_error = true;
			std::cerr << "[ERROR] Expected .ecsact file instead got'"
								<< file_path.string() << "'\n";
		} else if(!fs::exists(file_path)) {
			files_error = true;
			std::cerr << "[ERROR] '" << file_path.string() << "' does not exist\n";
		}
	}

	auto                  default_plugins_dir = get_default_plugins_dir();
	std::vector<fs::path> plugin_paths;
	std::vector<boost::dll::shared_library> plugins;
	bool                                    plugins_not_found = false;
	bool                                    invalid_plugins = false;

	for(auto plugin_arg : args.at("--plugin").asStringList()) {
		auto checked_plugin_paths = std::vector<fs::path>{};
		auto plugin_path = resolve_plugin_path(
			plugin_arg,
			default_plugins_dir,
			checked_plugin_paths
		);

		if(plugin_path) {
			boost::system::error_code ec;
			auto&                     plugin = plugins.emplace_back();
			plugin.load(plugin_path->string(), ec);
			auto validate_result = ecsact::codegen::plugin_validate(*plugin_path);
			if(validate_result.ok()) {
				plugin_paths.emplace_back(*plugin_path);
			} else {
				invalid_plugins = true;
				plugins.pop_back();
				std::cerr << "[ERROR] Plugin validation failed for '" << plugin_arg
									<< "'\n";

				for(auto err : validate_result.errors) {
					std::cerr << " - " << to_string(err) << "\n";
				}
			}
		} else {
			plugins_not_found = true;
			std::cerr //
				<< "[ERROR] Unable to find codegen plugin '" << plugin_arg
				<< "'. Paths checked:\n";

			for(auto& checked_path : checked_plugin_paths) {
				std::cerr << " - " << fs::absolute(checked_path).string() << "\n";
			}
		}
	}

	if(invalid_plugins || plugins_not_found || files_error) {
		return 1;
	}

	auto eval_errors = ecsact::eval_files(files);
	if(!eval_errors.empty()) {
		for(auto& eval_err : eval_errors) {
			std::cerr //
				<< "[ERROR] " << files[eval_err.source_index].string() << ":"
				<< eval_err.line << ":" << eval_err.character << " "
				<< eval_err.error_message << "\n";
		}
		return 1;
	}

	std::vector<ecsact_package_id> package_ids;
	package_ids.resize(static_cast<int32_t>(ecsact_meta_count_packages()));
	ecsact_meta_get_package_ids(
		static_cast<int32_t>(package_ids.size()),
		package_ids.data(),
		nullptr
	);

	std::unordered_set<std::string> output_paths;
	bool                            has_plugin_error = false;

	for(auto& plugin : plugins) {
		auto plugin_fn =
			plugin.get<decltype(ecsact_codegen_plugin)>("ecsact_codegen_plugin");
		auto plugin_name_fn = plugin.get<decltype(ecsact_codegen_plugin_name)>(
			"ecsact_codegen_plugin_name"
		);
		std::string plugin_name = plugin_name_fn();

		decltype(&ecsact_dylib_has_fn) dylib_has_fn = nullptr;

		if(plugin.has("ecsact_dylib_has_fn")) {
			dylib_has_fn =
				plugin.get<decltype(ecsact_dylib_has_fn)>("ecsact_dylib_has_fn");
		}

		auto dylib_set_fn_addr =
			plugin.get<decltype(ecsact_dylib_set_fn_addr)>("ecsact_dylib_set_fn_addr"
			);

		auto set_meta_fn_ptr = [&](const char* fn_name, auto fn_ptr) {
			if(dylib_has_fn && !dylib_has_fn(fn_name)) {
				return;
			}
			dylib_set_fn_addr(fn_name, reinterpret_cast<void (*)()>(fn_ptr));
		};

#define CALL_SET_META_FN_PTR(fn_name, unused) \
	set_meta_fn_ptr(#fn_name, &::fn_name)
		FOR_EACH_ECSACT_META_API_FN(CALL_SET_META_FN_PTR);

		std::optional<fs::path> outdir;
		if(args.at("--outdir").isString()) {
			outdir = fs::path(args.at("--outdir").asString());
			if(!fs::exists(*outdir)) {
				std::error_code ec;
				fs::create_directories(*outdir, ec);
				if(ec) {
					std::cerr //
						<< "[ERROR] Failed to create out directory " << outdir->string()
						<< ": " << ec.message() << "\n";
					return 3;
				}
			}
		}

		if(args.at("--stdout").asBool()) {
			plugin_fn(*package_ids.begin(), &stdout_write_fn);
			std::cout.flush();
		} else {
			for(auto package_id : package_ids) {
				fs::path output_file_path = ecsact_meta_package_file_path(package_id);
				if(output_file_path.empty()) {
					std::cerr //
						<< "[ERROR] Could not find package source file path from "
						<< "'ecsact_meta_package_file_path'\n";
					continue;
				}

				output_file_path.replace_extension(
					output_file_path.extension().string() + "." + plugin_name
				);

				if(outdir) {
					output_file_path = *outdir / output_file_path.filename();
				}

				if(output_paths.contains(output_file_path.string())) {
					has_plugin_error = true;
					std::cerr //
						<< "[ERROR] Plugin '" << plugin.location().filename().string()
						<< "' has conflicts with another plugin output file '"
						<< output_file_path.string() << "'\n";
					continue;
				}

				output_paths.emplace(output_file_path.string());
				if(fs::exists(output_file_path)) {
					fs::permissions(output_file_path, fs::perms::all);
				}
				file_write_stream.open(output_file_path);
				plugin_fn(package_id, &file_write_fn);
				file_write_stream.flush();
				file_write_stream.close();
				fs::permissions(output_file_path, file_readonly_perms);
			}
		}

		plugin.unload();
	}

	if(has_plugin_error) {
		return 2;
	}

	return 0;
}
