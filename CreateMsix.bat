@echo off

set /p "PASSWORD=Password: "

MSIXHeroCLI pack -d dist -p EcsactSdk.msix
MSIXHeroCLI sign --file %USERPROFILE%\Documents\Certificates\Seaube.pfx -p %PASSWORD% EcsactSdk.msix
