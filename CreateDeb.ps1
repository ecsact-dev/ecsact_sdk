#!/usr/bin/env pwsh

param (
	[Parameter(Mandatory)] $Version
)

$ErrorActionPreference = 'Stop'

$DebPath = "ecsact_sdk_$($Version)_amd64.deb"

try {
	((Get-Content -path ./dist/DEBIAN/control -Raw) -replace '0.0.0-placeholder',"$($Version)") | Set-Content -Path ./dist/DEBIAN/control
	rm -rf ./dist/images
	rm ./dist/AppxManifest.xml
	rm -f ./dist/bin/ecsact_rtb.runfiles/ecsact_cli/ecsact
	mkdir ./dist/usr
	mv ./dist/bin ./dist/usr/bin
	mv ./dist/share ./dist/usr/share
	mv ./dist/include ./dist/usr/include
	dpkg-deb --build --root-owner-group ./dist $DebPath
} finally {
	git checkout ./dist/DEBIAN/control
	git checkout ./dist/AppxManifest.xml
}

