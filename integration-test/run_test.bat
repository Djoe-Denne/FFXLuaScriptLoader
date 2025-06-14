@echo off
echo =========================================
echo     Integration Test for DLL Injection
echo =========================================
echo.

REM Check if required files exist
echo Checking required files...
if not exist test_app.exe (
    echo X Missing: test_app.exe
    echo Please build the project first.
    pause
    exit /b 1
)
echo + Found: test_app.exe

if not exist app_hook.dll (
    echo X Missing: app_hook.dll
    echo Please build the project first.
    pause
    exit /b 1
)
echo + Found: app_hook.dll

if not exist tasks\test_plugin.dll (
    echo X Missing: tasks\test_plugin.dll
    echo Please build the project first.
    pause
    exit /b 1
)
echo + Found: tasks\test_plugin.dll

if not exist app_injector.exe (
    echo X Missing: app_injector.exe
    echo Please build the project first.
    pause
    exit /b 1
)
echo + Found: app_injector.exe
echo.

echo Starting test application...
start /B test_app.exe
echo Test app started. Waiting for initialization...
timeout /t 2 /nobreak >nul
echo.

echo Running injector to inject main DLL and plugins...
echo NOTE: Injector will look for plugins in tasks/ directory
app_injector.exe test_app.exe app_hook.dll

if errorlevel 1 (
    echo X Injector failed!
    echo Check logs/app_hook.log for details.
    echo.
    pause
    exit /b 1
)

echo Injection completed successfully!
echo.
echo The test app should terminate automatically if plugins work correctly.
echo Waiting up to 10 seconds for test completion...
echo.

REM Wait and check if test_app.exe is still running
for /L %%i in (1,1,10) do (
    tasklist /FI "IMAGENAME eq test_app.exe" 2>NUL | find /I "test_app.exe" >NUL
    if errorlevel 1 (
        echo TEST PASSED! App terminated successfully after %%i seconds.
        echo - DLL injection successful
        echo - Plugin loading successful  
        echo - Task execution successful
        echo - Termination signal received
        goto :success
    )
    echo Waiting... (%%i/10)
    timeout /t 1 /nobreak >nul
)

echo TEST FAILED! App did not terminate within timeout.
echo This indicates plugin injection or task execution is not working.
echo Check logs/app_hook.log for detailed information.
echo.
REM Kill any remaining test processes
taskkill /F /IM test_app.exe 2>nul
goto :end

:success
echo.
echo Integration test completed successfully!
echo Check logs/app_hook.log for detailed logs.

:end
echo.
pause 