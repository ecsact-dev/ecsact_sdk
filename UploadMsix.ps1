#!/usr/bin/env pwsh

param (
	[Parameter(Mandatory)] $CertPath,
	[Parameter(Mandatory)][SecureString] $CertPassword
)

$ErrorActionPreference = 'Stop'

# GitHub requires TLS 1.2
[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12

$LatestTag = (gh release list -L 1).split()[0]
Write-Host "Latest Tag: $LatestTag"

$ArchiveUrl = "https://github.com/ecsact-dev/ecsact_sdk/releases/download/$($LatestTag)/ecsact_sdk_$($LatestTag)_windows_x64.zip"
$ArchivePath = "ecsact_sdk_$($LatestTag)_windows_x64.zip"

Write-Host "Downloading $ArchiveUrl to $ArchivePath..."
Invoke-WebRequest $ArchiveUrl -OutFile $ArchivePath -UseBasicParsing

git clean dist -dfx
Write-Host "Extracting $ArchivePath to dist..."
Expand-Archive -Path $ArchivePath -DestinationPath dist

((Get-Content -path .\ecsact_sdk.appinstaller -Raw) -replace '0.0.0.0-placeholder',"$($LatestTag).0") | Set-Content -Path .\ecsact_sdk.appinstaller
((Get-Content -path .\ecsact_sdk.appinstaller -Raw) -replace '0.0.0-placeholder',"$($LatestTag)") | Set-Content -Path .\ecsact_sdk.appinstaller

try {
	bazel run --ui_event_filters=-info --noshow_progress //:copy_dist_images
	. .\CreateMsix.ps1 -CertPath $CertPath -Version $LatestTag -CertPassword $CertPassword
	gh release upload $LatestTag $MsixPath ecsact_sdk.appinstaller --clobber
} finally {
	git checkout .\ecsact_sdk.appinstaller
}
