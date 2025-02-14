load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_file")

ECSACT_UNREAL_VERSION = "0.2.2"

def _prebuilt_tools(mctx):
    http_file(
        name = "ecsact_unreal_codegen_win64",
        downloaded_file_path = "EcsactUnrealCodegen.exe",
        executable = True,
        url = "https://github.com/ecsact-dev/ecsact_unreal/releases/download/{}/EcsactUnrealCodegen-Win64.exe".format(ECSACT_UNREAL_VERSION),
        integrity = "sha256-ynFQTRdnN4k7s3t2Z3dJ2fnxoQ8hV/P5y2cvgRMvxSg=",
    )
    http_file(
        name = "ecsact_unreal_codegen_linux",
        downloaded_file_path = "EcsactUnrealCodegen",
        executable = True,
        url = "https://github.com/ecsact-dev/ecsact_unreal/releases/download/{}/EcsactUnrealCodegen-Linux".format(ECSACT_UNREAL_VERSION),
        integrity = "sha256-nwcXevEQn1RJrm/7/Nq+HtR3nBLMbhlovfPSgXR923A=",
    )

prebuilt_tools = module_extension(_prebuilt_tools)
