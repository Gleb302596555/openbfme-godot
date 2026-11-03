@echo off
rem Builds the OpenBFME engine: GDExtension into godot\bin and the unit test runner.
rem build.bat [Release|Debug]; the default also builds tests and their helpers.
rem Needs Visual Studio 2022 (C++ workload), CMake and Ninja on PATH.
rem Load: the build runs at BELOW-NORMAL priority with at most OPENBFME_BUILD_JOBS parallel compile/link jobs (default: a third of the logical cores,
rem at least 2) so the machine stays usable while it builds. OPENBFME_BUILD_JOBS=2 is lighter, OPENBFME_BUILD_JOBS=0 lets Ninja use every core.
rem Queue: at most OPENBFME_BUILD_SLOTS builds (default 2) run on the machine, the others wait (tools\build_slot.ps1). OPENBFME_BUILD_SLOTS=1 is for a machine that lags.
rem Targets: OPENBFME_BUILD_TARGETS (default "openbfme openbfme_tests"); OPENBFME_BUILD_TARGETS=openbfme_tests skips the game's library when only tests are needed.
rem Test map: OPENBFME_TEST_COVERAGE=1 builds the tests into engine\build-cov with the marks tools\test_coverage_map.py reads; the normal build folder is not touched.
rem Compile cache: OPENBFME_COMPILER_CACHE=<full path to sccache.exe> compiles through it. Measured 2026-11-01: a tree at another path got 1 hit of 2483, so it does not speed up a new tree.
setlocal
set "ROOT=%~dp0"
set "BUILD_DIR=%ROOT%engine\build"
set "BUILD_TYPE=Release"
if not "%~1"=="" set "BUILD_TYPE=%~1"
set "COVERAGE=OFF"
if defined OPENBFME_TEST_COVERAGE (
    set "COVERAGE=ON"
    set "BUILD_DIR=%ROOT%engine\build-cov"
    set "OPENBFME_BUILD_TARGETS=openbfme_tests"
)
if not "%~2"=="" (
    echo ERROR: usage: build.bat [Release^|Debug]
    exit /b 2
)

if exist "%ROOT%engine\thirdparty\godot-cpp\CMakeLists.txt" goto have_godotcpp
git -C "%ROOT%." submodule update --init engine/thirdparty/godot-cpp
if errorlevel 1 exit /b 1
:have_godotcpp

where cl >nul 2>nul
if not errorlevel 1 goto have_msvc
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" goto no_msvc
for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSDIR=%%i"
if not defined VSDIR goto no_msvc
rem vcvars64 itself looks for vswhere on PATH
set "PATH=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer;%PATH%"
call "%VSDIR%\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 goto no_msvc
:have_msvc

cmake -S "%ROOT%engine" -B "%BUILD_DIR%" -G Ninja -DCMAKE_BUILD_TYPE=%BUILD_TYPE% -DOPENBFME_BUILD_TESTS=ON -DOPENBFME_TEST_COVERAGE=%COVERAGE% -DCMAKE_C_COMPILER=cl -DCMAKE_CXX_COMPILER=cl "-DCMAKE_C_COMPILER_LAUNCHER=%OPENBFME_COMPILER_CACHE%" "-DCMAKE_CXX_COMPILER_LAUNCHER=%OPENBFME_COMPILER_CACHE%"
if errorlevel 1 exit /b 1
if not defined OPENBFME_BUILD_JOBS set /a OPENBFME_BUILD_JOBS=%NUMBER_OF_PROCESSORS%/3
if %OPENBFME_BUILD_JOBS% lss 0 set "OPENBFME_BUILD_JOBS=2"
if %OPENBFME_BUILD_JOBS% equ 1 set "OPENBFME_BUILD_JOBS=2"
echo build jobs: %OPENBFME_BUILD_JOBS% (0 = all cores), priority below normal
if not defined OPENBFME_BUILD_TARGETS set "OPENBFME_BUILD_TARGETS=openbfme openbfme_tests"
"%SystemRoot%\System32\WindowsPowerShell\v1.0\powershell.exe" -NoProfile -ExecutionPolicy Bypass -File "%ROOT%tools\build_slot.ps1" -BuildDir "%BUILD_DIR%" -Targets "%OPENBFME_BUILD_TARGETS%" -Jobs %OPENBFME_BUILD_JOBS%
if errorlevel 1 exit /b 1

rem The helper executables the tests drive (x87_oracle, retail_oracle, numeric/apt/hash drivers): incremental,
rem rebuilt only when older than their sources (tools\retail_oracle\helpers.json). The tests refuse stale ones.
call "%ROOT%tools\retail_oracle\build.bat"
if errorlevel 1 exit /b 1
echo BUILD OK: %OPENBFME_BUILD_TARGETS%, test helpers in tools\
exit /b 0

:no_msvc
echo ERROR: Visual Studio 2022 with the C++ x64 tools was not found (vswhere/vcvars64).
exit /b 1
