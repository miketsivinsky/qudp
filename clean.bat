@echo off

set DISABLE_QMAKE_DEFAULT_BUILD=1
set USE_VS_BUILD=0

if "%1"=="" (
	set TOPDIR=.
) else (
	set TOPDIR=%1
)

del %TOPDIR%\Makefile.*
del %TOPDIR%\src\Makefile.*
del %TOPDIR%\test\test_rx\Makefile.*
del %TOPDIR%\test\test_tx\Makefile.*

del %TOPDIR%\*.sln
del %TOPDIR%\src\*.vcxproj.*
del %TOPDIR%\test\test_rx\*.vcxproj.*
del %TOPDIR%\test\test_tx\*.vcxproj.*


rd  %TOPDIR%\build /S /Q 
rd  %TOPDIR%\bin /S /Q 

if %DISABLE_QMAKE_DEFAULT_BUILD%==0 (
	rd  %TOPDIR%\src\debug /S /Q 
	rd  %TOPDIR%\src\release /S /Q
	
	rd  %TOPDIR%\test\test_rx\debug /S /Q 
	rd  %TOPDIR%\test\test_rx\release /S /Q

	rd  %TOPDIR%\test\test_tx\debug /S /Q 
	rd  %TOPDIR%\test\test_tx\release /S /Q
)

if %USE_VS_BUILD%==1 (
	rd  %TOPDIR%\src\x64 /S /Q
	rd  %TOPDIR%\test\test_rx\x64 /S /Q
	rd  %TOPDIR%\test\test_tx\x64 /S /Q
)
