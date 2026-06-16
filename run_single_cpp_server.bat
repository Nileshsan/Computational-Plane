@echo off
REM Run the single C++ payment analyzer executable on port 8888
REM Usage: run_single_cpp_server.bat [port]

setlocal
set "PORT=%~1"
if "%PORT%"=="" set "PORT=8888"

pushd "%~dp0build\Release" >nul 2>&1 || (
    echo ERROR: Release folder not found. Build the project first (run build_and_deploy.bat)
    endlocal
    exit /b 1
)

echo Starting cpp_payment_analyzer.exe on port %PORT%...
if exist cpp_payment_analyzer.exe (
    cpp_payment_analyzer.exe --port %PORT%
) else (
    echo ERROR: cpp_payment_analyzer.exe not found in %cd%
    echo Build the project first (run build_and_deploy.bat)
    popd
    endlocal
    exit /b 1
)

popd
endlocal
