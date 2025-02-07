load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_file")

def _prebuilt_tools(mctx):
    http_file(
        name = "ecsact_unreal_codegen_win64",
        downloaded_file_path = "EcsactUnrealCodegen.exe",
        executable = True,
        integrity = "sha256-HIra76wJlgsBAR+Oaj90INFCYLIS3QyPSWDDhvqUFCo=",
        url = "https://github.com/ecsact-dev/ecsact_unreal/releases/download/0.2.0/EcsactUnrealCodegen-Win64.exe",
    )
    http_file(
        name = "ecsact_unreal_codegen_linux",
        downloaded_file_path = "EcsactUnrealCodegen",
        executable = True,
        url = "https://github.com/ecsact-dev/ecsact_unreal/releases/download/0.2.0/EcsactUnrealCodegen-Linux",
    )

prebuilt_tools = module_extension(_prebuilt_tools)
