#!/usr/bin/env bash

set -e

bazel build --config=ci //:copy_dist_bin //:copy_dist_include //:copy_dist_codegen_plugins

bazel run --config=ci //:copy_dist_bin
bazel run --config=ci //:copy_dist_include
bazel run --config=ci //:copy_dist_codegen_plugins

ls -R dist

echo

[ ! -d "dist/include/dist" ] && echo "Missing ecsact include directory" && exit 1
[ ! -f "dist/include/bin/ecsact" ] && echo "Missing ecsact CLI" && exit 1