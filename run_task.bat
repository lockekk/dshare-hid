@echo off
setlocal

:ProcessArgs
if "%~1" neq "" (
    call :ProcessInput "%~1"
    exit /b %ERRORLEVEL%
)

:Interactive

echo ==================================================
echo            Deskflow Interactive Build
echo ==================================================
echo   1^) Build (Compile Only)
echo   2^) Configure Pristine (Clean ^& Reconfigure)
echo   3^) Launch
echo   4^) Configure Release (Clean ^& Config with Production Keys)
echo   5^) Build Deploy (Configure Release ^& Package)
echo   q^) Quit
echo.
set "input="
set /p input="Select a task (1-5 or q): "
if not defined input goto Interactive

if /i "%input%"=="q" exit /b 0
if /i "%input%"=="quit" exit /b 0
if /i "%input%"=="exit" exit /b 0

call :ProcessInput "%input%"

goto Interactive

:ProcessInput
set choice=%~1
if "%choice%"=="1" ( call :Build & exit /b 0 )
if "%choice%"=="build" ( call :Build & exit /b 0 )
if "%choice%"=="2" ( call :Pristine & exit /b 0 )
if "%choice%"=="build pristine" ( call :Pristine & exit /b 0 )
if "%choice%"=="3" ( call :Launch & exit /b 0 )
if "%choice%"=="launch" ( call :Launch & exit /b 0 )
if "%choice%"=="4" ( call :Release & exit /b 0 )
if "%choice%"=="release" ( call :Release & exit /b 0 )
if "%choice%"=="5" ( call :Deploy & exit /b 0 )
if "%choice%"=="build deploy" ( call :Deploy & exit /b 0 )
echo Invalid selection: %choice%
exit /b 1

:Build
call :SetupEnv
if not exist build (
    echo Error: 'build' directory not found. Please run a configuration step (2 or 4^) first.
    exit /b 1
)
echo --- BUILDING ---
cmake --build build -j%NUMBER_OF_PROCESSORS%
exit /b %ERRORLEVEL%

:Pristine
call :SetupEnv
echo --- CLEANING AND RECONFIGURING (PRISTINE) ---
if exist build rd /s /q build
call :Configure
echo Configuration complete. Run option 1 to build.
exit /b 0

:Release
call :SetupEnv
echo --- CLEANING AND RECONFIGURING (RELEASE) ---
if exist build rd /s /q build
echo --- CONFIGURING ---
cmake -S . -B build -G "Ninja" -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake -DVCPKG_MANIFEST_MODE=ON -DVCPKG_TARGET_TRIPLET=x64-windows -DVCPKG_QT=ON -DBUILD_TESTS=OFF -DDESKFLOW_PAYPAL_ACCOUNT="%PAYPAL_ACCOUNT%" -DDESKFLOW_PAYPAL_URL="%PAYPAL_URL%" -DDESKFLOW_CDC_PUBLIC_KEY="%DESKFLOW_CDC_PUBLIC_KEY%" -DDESKFLOW_ESP32_ENCRYPTION_KEY="%DESKFLOW_ESP32_ENCRYPTION_KEY%"
echo Release Configuration complete. Run option 1 to build.
exit /b %ERRORLEVEL%

:Deploy
call :Release
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%
call :Build
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%
echo --- PACKAGING ---
cmake --build build --target package
exit /b %ERRORLEVEL%

:Launch
if not exist build\bin\deskflow-hid.exe (
    echo Error: Application not built. Select '1' to build first.
    exit /b 1
)
echo --- LAUNCHING DESKFLOW-HID ---
build\bin\deskflow-hid.exe
exit /b 0

:Configure
echo --- CONFIGURING ---
cmake -S . -B build -G "Ninja" -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake -DVCPKG_MANIFEST_MODE=ON -DVCPKG_TARGET_TRIPLET=x64-windows -DVCPKG_QT=ON -DBUILD_TESTS=OFF -DDESKFLOW_PAYPAL_ACCOUNT="%PAYPAL_ACCOUNT%" -DDESKFLOW_PAYPAL_URL="%PAYPAL_URL%"
exit /b %ERRORLEVEL%

:SetupEnv
where cl >nul 2>nul
if %ERRORLEVEL% neq 0 (
    call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
)
exit /b 0
