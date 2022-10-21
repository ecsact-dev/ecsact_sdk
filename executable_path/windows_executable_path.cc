#include "executable_path.hh"

#include <windows.h>

namespace fs = std::filesystem;

fs::path executable_path::executable_path() {
	char   result[MAX_PATH];
	size_t length = GetModuleFileName(NULL, result, MAX_PATH);
	if(length == 0) {
		return fs::path{};
	}
	return fs::path(std::string(result, length));
}
