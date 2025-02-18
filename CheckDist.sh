#!/usr/bin/env bash

set -e

bazel build --config=ci //:copy_dist_bin //:copy_dist_include //:copy_dist_codegen_plugins

bazel run --config=ci //:copy_dist_bin
bazel run --config=ci //:copy_dist_include
bazel run --config=ci //:copy_dist_codegen_plugins

find dist -type f

echo

[ ! -d "dist/include/ecsact" ] && echo "Missing ecsact include directory" && exit 1
[ ! -f "dist/bin/ecsact" ] && echo "Missing ecsact CLI" && exit 1
[ ! -f "dist/bin/ecsact_lsp_server" ] && echo "Missing ecsact lsp CLI" && exit 1
[ ! -f "dist/bin/EcsactUnrealCodegen" ] && echo "Missing EcsatUnrealCodegen tool" && exit 1

exit 0
