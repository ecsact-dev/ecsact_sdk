#!/usr/bin/env pwsh

$ErrorActionPreference = 'Stop'

# GitHub requires TLS 1.2
[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12

$LatestTag = (gh release list -L 1).split()[0]
Write-Host "Latest Tag: $LatestTag"

$ArchiveUrl = "https://github.com/ecsact-dev/ecsact_sdk/releases/download/$($LatestTag)/ecsact_sdk_$($LatestTag)_linux_x64.tar.gz"
$ArchivePath = "ecsact_sdk_$($LatestTag)_linux_x64.tar.gz"

Write-Host "Downloading $ArchiveUrl to $ArchivePath..."
Invoke-WebRequest $ArchiveUrl -OutFile $ArchivePath -UseBasicParsing

git clean dist -dfx
Write-Host "Extracting $ArchivePath to dist..."
tar -xf $ArchivePath -C dist

. .\CreateDeb.ps1 -Version $LatestTag
gh release upload $LatestTag $DebPath --clobber

