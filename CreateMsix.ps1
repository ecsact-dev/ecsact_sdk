#!/usr/bin/env pwsh

param ([Parameter(Mandatory)] $Version, [Parameter(Mandatory)][SecureString] $CertPassword)

$ErrorActionPreference = 'Stop'

$CertPasswordPlain = ConvertFrom-SecureString -SecureString $CertPassword -AsPlainText
$CertPath = "$env:USERPROFILE\Documents\Certificates\Seaube.pfx"

try {
	((Get-Content -path .\dist\AppxManifest.xml -Raw) -replace '0.0.0.0-placeholder',"$($Version).0") | Set-Content -Path .\dist\AppxManifest.xml
	$MsixPath = "ecsact_sdk_$($Version)_windows_x64.msix"
	MSIXHeroCLI pack -d dist -p $MsixPath
	MSIXHeroCLI sign --file $CertPath -p $CertPasswordPlain $MsixPath
} finally {
	git checkout .\dist\AppxManifest.xml
}
