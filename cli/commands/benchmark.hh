#pragma once

#include <type_traits>

#include "./command.hh"

namespace ecsact::cli::detail {

int benchmark_command(int argc, char* argv[]);
static_assert(std::is_same_v<command_fn_t, decltype(&benchmark_command)>);

} // namespace ecsact::cli::detail
