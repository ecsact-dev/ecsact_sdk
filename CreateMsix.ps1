#!/usr/bin/env pwsh

param (
	[Parameter(Mandatory)] $Version,
	[Parameter(Mandatory)][SecureString] $CertPassword,
	$CertPath = "$env:USERPROFILE\Documents\Certificates\EcsactDev.pfx"
)

$ErrorActionPreference = 'Stop'

$MakeAppxPath = "${env:ProgramFiles(x86)}\Windows Kits\10\bin\10.0.22621.0\x64\makeappx.exe"
$SignToolPath = "${env:ProgramFiles(x86)}\Windows Kits\10\bin\10.0.22621.0\x64\signtool.exe"

$CertPasswordPlain = ConvertFrom-SecureString -SecureString $CertPassword -AsPlainText

try {
	((Get-Content -path .\dist\AppxManifest.xml -Raw) -replace '0.0.0.0-placeholder',"$($Version).0") | Set-Content -Path .\dist\AppxManifest.xml
	$MsixPath = "ecsact_sdk_$($Version)_windows_x64.msix"

	& $SignToolPath sign `
		/debug /fd SHA384 `
		/f $CertPath `
		/p $CertPasswordPlain `
		/tr http://timestamp.sectigo.com /td SHA256 `
		.\dist\bin\ecsact.exe `
		.\dist\bin\ecsact_rtb.exe

	if(-not $?) {
		throw "Signing dist binaries failed with exit code ${LastExitCode}"
	}

	& $MakeAppxPath pack /v /o /h SHA384 /d .\dist\ /p $MsixPath

	if(-not $?) {
		throw "$MakeAppxPath exited with ${LastExitCode}"
	}

	& $SignToolPath sign `
		/debug /fd SHA384 `
		/f $CertPath `
		/p $CertPasswordPlain `
		/tr http://timestamp.sectigo.com /td SHA256 `
		$MsixPath
	if(-not $?) {
		throw "Signing ${MsixPath} failed with exit code ${LastExitCode}"
	}
} finally {
	git checkout .\dist\AppxManifest.xml
}
