@echo off
REM Clear VSCode C++ IntelliSense database to force refresh
REM Run this script if you see "Include file not found in browse.path" errors

setlocal enabledelayedexpansion

set BUILD_DIR=build
set DB_FILE=build\.vscode-cpptools-db

echo.
echo ======================================================================
echo    Clearing C++ IntelliSense Database
echo ======================================================================
echo.

if exist "%DB_FILE%" (
    echo [INFO] Deleting IntelliSense database: %DB_FILE%
    rmdir /s /q "%DB_FILE%" 2>nul
    if exist "%DB_FILE%" (
        echo [ERROR] Failed to delete database
        goto error
    )
    echo [OK] Database deleted
) else (
    echo [INFO] Database not found (will be recreated)
)

REM VSCode will auto-recreate the database on next startup
echo.
echo [OK] IntelliSense database cleared!
echo.
echo Next steps:
echo 1. Close VSCode
echo 2. Reopen the C++ project folder
echo 3. VSCode will automatically rebuild the IntelliSense database
echo 4. Wait 2-3 minutes for the database to rebuild
echo.
echo The "Include file not found" errors should disappear after rebuild.
echo.
goto success

:error
echo.
echo [ERROR] Failed to clear IntelliSense database
exit /b 1

:success
echo [SUCCESS] IntelliSense database ready for rebuild
exit /b 0
