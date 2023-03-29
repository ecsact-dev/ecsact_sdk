@echo off

for /f %%i in ('git describe --tags --abbrev^=0') do set GIT_TAG=%%i
echo STABLE_GIT_TAG %GIT_TAG%

