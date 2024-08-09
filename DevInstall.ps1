#!/usr/bin/env pwsh

param (
	[Parameter(Mandatory)][SecureString] $CertPassword,
	$CertPath = [Environment]::GetFolderPath("MyDocuments") + "\Certificates\EcsactDev.pfx"
)

$ErrorActionPreference = 'Stop'

$GitTag = git describe --tags --abbrev=0
$GitTagSplit = $GitTag.Split(".")
$IncrementedVersion = $GitTagSplit[0] + "." + $GitTagSplit[1] + "." + (([int]$GitTagSplit[2]) + 1)

# Simulate a release to set the correct version
$Env:GITHUB_REF=$IncrementedVersion

Write-Output 'Starting DevInstall' *> DevInstall.log

. .\CopyDist.ps1

if ([System.Environment]::OSVersion.Platform -eq 'Unix')
{
	# TODO(zaucy): Check if developer machine can install .deb files and give
	# a nice error if they can't.
	. ./CreateDeb.ps1 -Version $IncrementedVersion
	sudo dpkg -i $DebPath
} else
{
	$ExistingPackage = Get-AppPackage EcsactSdk

	if ($ExistingPackage)
	{
		Remove-AppPackage -Package $ExistingPackage.PackageFullName *>> DevInstall.log
	}

	. .\CreateMsix.ps1 -Version $IncrementedVersion -CertPassword $CertPassword -CertPath $CertPath
	Add-AppPackage -Path $MsixPath *>> DevInstall.log

	Write-Progress -Status "DevInstall Success" -Activity "$MsixPath Installed"
	Write-Host "Successfully installed Ecsact SDK $Version" -ForegroundColor Green
}

