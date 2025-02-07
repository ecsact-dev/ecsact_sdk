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

try
{
	((Get-Content -path .\dist\AppxManifest.xml -Raw) -replace '0.0.0.0-placeholder',"$($Version).0") | Set-Content -Path .\dist\AppxManifest.xml
	$MsixPath = "ecsact_sdk_$($Version)_windows_x64.msix"

	Write-Output "Creating $MsixPath" *> CreateMsix.log

	Copy-Item ".\dist\images\ecsact-color44.png" ".\dist\images\ecsact-color44.targetsize-44_altform-unplated.png" -Force
	Copy-Item ".\dist\images\ecsact-color150.png" ".\dist\images\ecsact-color150.targetsize-150_altform-unplated.png" -Force

	& $MakePriPath createconfig /cf .\dist\priconfig.xml /dq en-US /o *>> CreateMsix.log
	if(-not $?)
	{
		throw "$MakePriPath createconfig failed with exit code ${LastExitCode}"
	}

	& $MakePriPath new /pr .\dist /cf .\dist\priconfig.xml /dq en-US /o /OutputFile ".\dist\resources.pri" *>> CreateMsix.log
	if(-not $?)
	{
		throw "$MakePriPath new failed with exit code ${LastExitCode}"
	}

	$ExecutablesToSign = @(
		'.\dist\bin\ecsact.exe',
		'.\dist\bin\ecsact_lsp_server.exe',
		'.\dist\bin\EcsactUnrealCodegen.exe'
	)

	foreach($Executable in $ExecutablesToSign)
	{
		Set-ItemProperty -Path $Executable -Name IsReadOnly -Value $false
	}

	foreach($Executable in $ExecutablesToSign)
	{
		Write-Progress -Status "Signing distribution files" -Activity "$Executable"

		& $SignToolPath sign `
			/debug /fd SHA384 `
			/f $CertPath `
			/p $CertPasswordPlain `
			/tr http://timestamp.sectigo.com /td SHA256 `
			$Executable *>> CopyMsix.log

		if(-not $?)
		{
			throw "Signing dist binaries failed with exit code ${LastExitCode}"
		}
	}

	foreach($Executable in $ExecutablesToSign)
	{
		Set-ItemProperty -Path $Executable -Name IsReadOnly -Value $true
	}

	& $MakeAppxPath pack /v /o /h SHA384 /d .\dist\ /p $MsixPath *>> CopyMsix.log

	if(-not $?)
	{
		throw "$MakeAppxPath exited with ${LastExitCode}"
	}

	Write-Progress -Status "Signing distribution files" -Activity "$MsixPath"

	& $SignToolPath sign `
		/debug /fd SHA384 `
		/f $CertPath `
		/p $CertPasswordPlain `
		/tr http://timestamp.sectigo.com /td SHA256 `
		$MsixPath *>> CopyMsix.log
	if(-not $?)
	{
		throw "Signing ${MsixPath} failed with exit code ${LastExitCode}"
	}
} finally
{
	git checkout .\dist\AppxManifest.xml *>> CopyMsix.log
}
