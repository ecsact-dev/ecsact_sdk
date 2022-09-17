#!/usr/bin/env pwsh

$ErrorActionPreference = 'Stop'

git clean dist -fdx
bazel run --ui_event_filters=-info,-stdout,-stderr --noshow_progress //:copy_dist_include
bazel run --ui_event_filters=-info,-stdout,-stderr --noshow_progress //:copy_runfiles
bazel run --ui_event_filters=-info,-stdout,-stderr --noshow_progress //:copy_dist_bin
bazel run --ui_event_filters=-info,-stdout,-stderr --noshow_progress //:copy_dist_codegen_plugins
