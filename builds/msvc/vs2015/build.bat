@if "%ECHOON%" == "" (@echo off) else (@echo %ECHOON%)&:: set ECHOON=on if you want to debug this script
@::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
@::  THIS FILE IS 100% GENERATED BY ZPROJECT; DO NOT EDIT EXCEPT EXPERIMENTALLY  ::
@::  Read the zproject/README.md for information about making permanent changes. ::
@::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
:: Usage:     build.bat [Clean]
@setlocal

:: make sure our directory is correct for building
@pushd %~dp0%

:: supports passing in Clean as third argument if "make clean" behavior is desired
SET action=Building
SET target=%1
if NOT "%target%" == "" set target=/t:%target%&set action=Cleaning

SET solution=libsphactor.sln
SET version=14
SET log=build.log
SET tools=Microsoft Visual Studio %version%.0\VC\vcvarsall.bat
if "%version%" == "15" SET tools=Microsoft Visual Studio\2017\Community\VC\Auxiliary\Build\vcvarsall.bat
if "%version%" == "16" SET tools=Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvarsall.bat
SET environment="%programfiles(x86)%\%tools%"
IF NOT EXIST %environment% SET environment="%programfiles%\%tools%"
IF NOT EXIST %environment% GOTO no_tools

SET packages=
IF EXIST "..\..\..\..\libzmq\builds/msvc/vs2015\libzmq.import.props" (
    COPY /Y "..\..\..\..\libzmq\builds/msvc/vs2015\libzmq.import.props" . > %log%
    IF errorlevel 1 GOTO error
) ELSE (
    ECHO Did not find libzmq, aborting.
    ECHO Please clone from https://github.com/zeromq/libzmq.git, and then build.
    GOTO error
)
IF EXIST "..\..\..\..\czmq\builds/msvc/vs2015\czmq.import.props" (
    COPY /Y "..\..\..\..\czmq\builds/msvc/vs2015\czmq.import.props" . > %log%
    IF errorlevel 1 GOTO error
) ELSE (
    ECHO Did not find czmq, aborting.
    ECHO Please clone from https://github.com/zeromq/czmq.git, and then build.
    GOTO error
)

ECHO %action% libsphactor... (%packages%)

:: save original path
@set oldpath=%PATH%

:: set correct environment for build target
CALL %environment% x86 >> %log%
@if "%ECHOON%" == "" (@echo off) else (@echo %ECHOON%)&:: set ECHOON=on if you want to debug this script
ECHO Platform=x86

ECHO Configuration=DynDebug
msbuild /m /v:n /p:Configuration=DynDebug /p:Platform=Win32 %packages% %solution% %target% >> %log%
IF errorlevel 1 GOTO error
ECHO Configuration=DynRelease
msbuild /m /v:n /p:Configuration=DynRelease /p:Platform=Win32 %packages% %solution% %target% >> %log%
IF errorlevel 1 GOTO error
ECHO Configuration=LtcgDebug
msbuild /m /v:n /p:Configuration=LtcgDebug /p:Platform=Win32 %packages% %solution% %target% >> %log%
IF errorlevel 1 GOTO error
ECHO Configuration=LtcgRelease
msbuild /m /v:n /p:Configuration=LtcgRelease /p:Platform=Win32 %packages% %solution% %target% >> %log%
IF errorlevel 1 GOTO error
ECHO Configuration=StaticDebug
msbuild /m /v:n /p:Configuration=StaticDebug /p:Platform=Win32 %packages% %solution% %target% >> %log%
IF errorlevel 1 GOTO error
ECHO Configuration=StaticRelease
msbuild /m /v:n /p:Configuration=StaticRelease /p:Platform=Win32 %packages% %solution% %target% >> %log%
IF errorlevel 1 GOTO error

:: restore original path
@set PATH=%oldpath%

:: set correct environment for build target
CALL %environment% x86_amd64 >> %log%
@if "%ECHOON%" == "" (@echo off) else (@echo %ECHOON%)&:: set ECHOON=on if you want to debug this script
ECHO Platform=x64

ECHO Configuration=DynDebug
msbuild /m /v:n /p:Configuration=DynDebug /p:Platform=x64 %packages% %solution% %target% >> %log%
IF errorlevel 1 GOTO error
ECHO Configuration=DynRelease
msbuild /m /v:n /p:Configuration=DynRelease /p:Platform=x64 %packages% %solution% %target% >> %log%
IF errorlevel 1 GOTO error
ECHO Configuration=LtcgDebug
msbuild /m /v:n /p:Configuration=LtcgDebug /p:Platform=x64 %packages% %solution% %target% >> %log%
IF errorlevel 1 GOTO error
ECHO Configuration=LtcgRelease
msbuild /m /v:n /p:Configuration=LtcgRelease /p:Platform=x64 %packages% %solution% %target% >> %log%
IF errorlevel 1 GOTO error
ECHO Configuration=StaticDebug
msbuild /m /v:n /p:Configuration=StaticDebug /p:Platform=x64 %packages% %solution% %target% >> %log%
IF errorlevel 1 GOTO error
ECHO Configuration=StaticRelease
msbuild /m /v:n /p:Configuration=StaticRelease /p:Platform=x64 %packages% %solution% %target% >> %log%
IF errorlevel 1 GOTO error

ECHO %action% complete: %packages% %solution%
GOTO end

:error
ECHO *** ERROR, build terminated early: see %log%
GOTO end

:no_tools
ECHO *** ERROR, build tools not found: %tools%

:end
:: restore original path
if NOT "%oldpath%" == "" @set PATH=%oldpath%
popd
@endlocal
