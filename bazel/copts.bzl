copts = select({
    "@bazel_tools//tools/cpp:msvc": ["/std:c++20"],
    "//conditions:default": ["-std=c++20"],
})
