#!/usr/bin/env pwsh

$ErrorActionPreference = 'Stop'

$RunCopyTargets = @(
	'//:copy_dist_include',
	'//:copy_dist_bin',
	'//:copy_dist_codegen_plugins',
	'//:copy_dist_images',
	'//:copy_dist_recipe_bundles'
);

git clean dist -fdx *> CopyDist.log

Write-Progress -Status "Building copy targets..." -Activity "Copying distribution files"
bazel build $RunCopyTargets -k *>> CopyDist.log

foreach($Target in $RunCopyTargets)
{
	Write-Progress -Status "$Target" -Activity "Copying distribution files"
	bazel run --ui_event_filters=-info --noshow_progress $Target *>> CopyDist.log
	if(-not $?)
	{
		throw "$Target failed. Check ./CopyDist.log for more details."
	}
}
