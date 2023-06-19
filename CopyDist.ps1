#!/usr/bin/env pwsh

$ErrorActionPreference = 'Stop'

git clean dist -fdx
bazel build //:copy_dist_include //:copy_runfiles //:copy_dist_bin //:copy_dist_codegen_plugins //:copy_dist_images
bazel run --ui_event_filters=-info --noshow_progress //:copy_dist_include
bazel run --ui_event_filters=-info --noshow_progress //:copy_dist_images
bazel run --ui_event_filters=-info --noshow_progress //:copy_runfiles
bazel run --ui_event_filters=-info --noshow_progress //:copy_dist_bin
bazel run --ui_event_filters=-info --noshow_progress //:copy_dist_codegen_plugins
