#!/usr/bin/env pwsh

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

((Get-Content -path .\dist\AppxManifest.xml -Raw) -replace '0.0.0.0-placeholder',"$($LatestTag).0") | Set-Content -Path .\dist\AppxManifest.xml

try {
	$MsixPath = "ecsact_sdk_$($LatestTag)_windows_x64.msix"
	$CertPassword = Read-Host "Cert Password"
	MSIXHeroCLI pack -d dist -p $MsixPath
	MSIXHeroCLI sign --file $env:USERPROFILE\Documents\Certificates\Seaube.pfx -p $CertPassword $MsixPath
	gh release upload $LatestTag $MsixPath
} finally {
	git checkout .\dist\AppxManifest.xml
}
