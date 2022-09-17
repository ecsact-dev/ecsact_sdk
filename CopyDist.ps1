#!/usr/bin/env pwsh

$ErrorActionPreference = 'Stop'

git clean dist -fdx
bazel run //:copy_dist_include
bazel run //:copy_runfiles
bazel run //:copy_dist_bin
bazel run //:copy_dist_codegen_plugins
