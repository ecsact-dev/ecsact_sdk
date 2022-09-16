#!/usr/bin/env bash

set -e

bazel build --config=ci //:copy_dist_bin //:copy_dist_include //:copy_dist_codegen_plugins

bazel run --config=ci //:copy_dist_bin
bazel run --config=ci //:copy_runfiles
bazel run --config=ci //:copy_dist_include
bazel run --config=ci //:copy_dist_codegen_plugins

find dist -type f

echo

[ ! -d "dist/include/ecsact" ] && echo "Missing ecsact include directory" && exit 1
[ ! -f "dist/bin/ecsact" ] && echo "Missing ecsact CLI" && exit 1
[ ! -f "dist/bin/ecsact_rtb" ] && echo "Missing ecsact runtime builder CLI" && exit 1
[ ! -d "dist/bin/ecsact_rtb.runfiles" ] && echo "Missing ecsact rtb runfiles" && exit 1

exit 0
