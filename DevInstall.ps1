#!/usr/bin/env pwsh

$ErrorActionPreference = 'Stop'

$GitTag = git describe --tags --abbrev=0
$GitTagSplit = $GitTag.Split(".")
$IncrementedVersion = $GitTagSplit[0] + "." + $GitTagSplit[1] + "." + (([int]$GitTagSplit[2]) + 1)

. .\CopyDist.ps1
. .\CreateMsix.ps1 -Version $IncrementedVersion

Add-AppPackage -path $MsixPath
