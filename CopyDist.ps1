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

$ProgressId = Get-Random
Write-Progress -Id $ProgressId -Status "Building copy targets..." -Activity "Copying distribution files"
bazel build $RunCopyTargets -k *>> CopyDist.log

$CompletedCount = 0

foreach($Target in $RunCopyTargets)
{
	$PcComplete = ($CompletedCount / $RunCopyTargets.Length) * 100
	Write-Progress -Id $ProgressId -Status "$Target" -Activity "($($CompletedCount + 1)/$($RunCopyTargets.Length)) Copying distribution files" -PercentComplete $PcComplete
	bazel run --ui_event_filters=-info --noshow_progress $Target *>> CopyDist.log
	if(-not $?)
	{
		throw "$Target failed. Check ./CopyDist.log for more details."
	}

	$CompletedCount += 1
}

Write-Progress -Id $ProgressId -Completed
