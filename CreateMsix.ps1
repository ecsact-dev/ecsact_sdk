#!/usr/bin/env pwsh

param (
	[Parameter(Mandatory)] $Version,
	[Parameter(Mandatory)][SecureString] $CertPassword,
	$CertPath = "$env:USERPROFILE\Documents\Certificates\EcsactDev.pfx"
)

$ErrorActionPreference = 'Stop'

$MakeAppxPath = "${env:ProgramFiles(x86)}\Windows Kits\10\bin\10.0.22621.0\x64\makeappx.exe"
$SignToolPath = "${env:ProgramFiles(x86)}\Windows Kits\10\bin\10.0.22621.0\x64\signtool.exe"
$MakePriPath = "${env:ProgramFiles(x86)}\Windows Kits\10\bin\10.0.22621.0\x64\makepri.exe"

$CertPasswordPlain = ConvertFrom-SecureString -SecureString $CertPassword -AsPlainText

try {
	((Get-Content -path .\dist\AppxManifest.xml -Raw) -replace '0.0.0.0-placeholder',"$($Version).0") | Set-Content -Path .\dist\AppxManifest.xml
	$MsixPath = "ecsact_sdk_$($Version)_windows_x64.msix"

	Copy-Item ".\dist\images\ecsact-color44.png" ".\dist\images\ecsact-color44.targetsize-44_altform-unplated.png" -Force
	Copy-Item ".\dist\images\ecsact-color150.png" ".\dist\images\ecsact-color150.targetsize-150_altform-unplated.png" -Force

	& $MakePriPath createconfig /cf .\dist\priconfig.xml /dq en-US /o
	if(-not $?) {
		throw "$MakePriPath createconfig failed with exit code ${LastExitCode}"
	}

	& $MakePriPath new /pr .\dist /cf .\dist\priconfig.xml /dq en-US /o /OutputFile ".\dist\resources.pri"
	if(-not $?) {
		throw "$MakePriPath new failed with exit code ${LastExitCode}"
	}

	Set-ItemProperty -Path .\dist\bin\ecsact.exe -Name IsReadOnly -Value $false
	Set-ItemProperty -Path .\dist\bin\ecsact_lsp_server.exe -Name IsReadOnly -Value $false

	& $SignToolPath sign `
		/debug /fd SHA384 `
		/f $CertPath `
		/p $CertPasswordPlain `
		/tr http://timestamp.sectigo.com /td SHA256 `
		.\dist\bin\ecsact.exe `
		.\dist\bin\ecsact_lsp_server.exe

	if(-not $?) {
		throw "Signing dist binaries failed with exit code ${LastExitCode}"
	}

	Set-ItemProperty -Path .\dist\bin\ecsact.exe -Name IsReadOnly -Value $true
	Set-ItemProperty -Path .\dist\bin\ecsact_lsp_server.exe -Name IsReadOnly -Value $true

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
