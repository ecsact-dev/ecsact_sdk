@echo off

if "%GITHUB_REF%"=="" (
	for /f %%i in ('git describe --tags --abbrev^=0') do (
		set GITHUB_REF=%%i
	)
)

echo STABLE_ECSACT_SDK_VERSION %GITHUB_REF%

