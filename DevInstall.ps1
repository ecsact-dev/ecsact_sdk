#!/usr/bin/env pwsh

param ([Parameter(Mandatory)][SecureString] $CertPassword)

$ErrorActionPreference = 'Stop'

$ExistingPackage = Get-AppPackage EcsactSdk

if ($ExistingPackage) {
	Remove-AppPackage -Package $ExistingPackage.PackageFullName
}

$GitTag = git describe --tags --abbrev=0
$GitTagSplit = $GitTag.Split(".")
$IncrementedVersion = $GitTagSplit[0] + "." + $GitTagSplit[1] + "." + (([int]$GitTagSplit[2]) + 1)

. .\CopyDist.ps1
. .\CreateMsix.ps1 -Version $IncrementedVersion -CertPassword $CertPassword

Add-AppPackage -Path $MsixPath
