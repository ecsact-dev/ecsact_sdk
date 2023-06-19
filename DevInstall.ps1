#!/usr/bin/env pwsh

param (
	[Parameter(Mandatory)][SecureString] $CertPassword,
	$CertPath = "$env:USERPROFILE\Documents\Certificates\EcsactDev.pfx"
)

$ErrorActionPreference = 'Stop'

$GitTag = git describe --tags --abbrev=0
$GitTagSplit = $GitTag.Split(".")
$IncrementedVersion = $GitTagSplit[0] + "." + $GitTagSplit[1] + "." + (([int]$GitTagSplit[2]) + 1)

. .\CopyDist.ps1

if ([System.Environment]::OSVersion.Platform == 'Unix') {
	# TODO(zaucy): Check if developer machine can install .deb files and give
	# a nice error if they can't.
	. ./CreateDeb.ps1 -Version $IncrementedVersion
	sudo dpkg -i $DebPath
} else {
	$ExistingPackage = Get-AppPackage EcsactSdk

	if ($ExistingPackage) {
		Remove-AppPackage -Package $ExistingPackage.PackageFullName
	}

	. .\CreateMsix.ps1 -Version $IncrementedVersion -CertPassword $CertPassword -CertPath $CertPath
	Add-AppPackage -Path $MsixPath
}

