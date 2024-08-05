#!/usr/bin/env pwsh

$ErrorActionPreference = 'Stop'

git clean dist -fdx
bazel build //:copy_dist_include //:copy_dist_bin //:copy_dist_codegen_plugins //:copy_dist_images -c opt
bazel run --ui_event_filters=-info --noshow_progress //:copy_dist_include -c opt
bazel run --ui_event_filters=-info --noshow_progress //:copy_dist_images -c opt
bazel run --ui_event_filters=-info --noshow_progress //:copy_dist_bin -c opt
bazel run --ui_event_filters=-info --noshow_progress //:copy_dist_codegen_plugins -c opt
bazel run --ui_event_filters=-info --noshow_progress //:copy_dist_recipe_bundles -c opt
