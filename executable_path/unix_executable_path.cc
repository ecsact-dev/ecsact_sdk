#include "executable_path.hh"

namespace fs = std::filesystem;

fs::path executable_path::executable_path() {
	std::error_code ec;
	auto            result = fs::canonical("/proc/self/exe", ec);
	if(ec) {
		result = fs::path{};
	}

	return result;
}
