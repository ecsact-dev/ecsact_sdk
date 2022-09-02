#!/usr/bin/env pwsh

$ErrorActionPreference = 'Stop'

$Version = Read-Host "Version"
try {
	((Get-Content -path .\dist\AppxManifest.xml -Raw) -replace '0.0.0.0-placeholder',"$($Version).0") | Set-Content -Path .\dist\AppxManifest.xml
	$MsixPath = "ecsact_sdk_$($Version)_windows_x64.msix"
	$CertPassword = Read-Host "Cert Password"
	MSIXHeroCLI pack -d dist -p $MsixPath
	MSIXHeroCLI sign --file $env:USERPROFILE\Documents\Certificates\Seaube.pfx -p $CertPassword $MsixPath
} finally {
	git checkout .\dist\AppxManifest.xml
}
