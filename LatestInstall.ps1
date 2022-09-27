#!/usr/bin/env pwsh

$ErrorActionPreference = 'Stop'

# GitHub requires TLS 1.2
[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12

$LatestTag = (gh release list -L 1).split()[0]
Write-Host "Latest Tag: $LatestTag"

$MsixPath = "ecsact_sdk_$($LatestTag)_windows_x64.msix"
$MsixDownloadUrl = "https://github.com/ecsact-dev/ecsact_sdk/releases/download/$($LatestTag)/$($MsixPath)"

if (-not(Test-Path -Path $MsixPath -PathType Leaf)) {
	Write-Host "Downloading $MsixDownloadUrl ..."
	Invoke-WebRequest $MsixDownloadUrl -OutFile $MsixPath -UseBasicParsing
}

$ExistingPackage = Get-AppPackage EcsactSdk

if ($ExistingPackage) {
	Write-Host "Removing old package $($ExistingPackage.PackageFullName) .."
	Remove-AppPackage -Package $ExistingPackage.PackageFullName
}

Write-Host "Adding new package $($MsixPath) ..."
Add-AppPackage -Path $MsixPath

$ExistingPackage = Get-AppPackage EcsactSdk

Write-Host "$($ExistingPackage.PackageFullName) installed!"
