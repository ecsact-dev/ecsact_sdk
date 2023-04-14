copts = select({
    "@bazel_tools//tools/cpp:msvc": ["/std:c++20", "/Zc:preprocessor", "/permissive-"],
    "//conditions:default": ["-std=c++20", "-Werror", "-fexperimental-library"],
})
