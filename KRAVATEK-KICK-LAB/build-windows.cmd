@echo off
setlocal
cd /d "%~dp0"
where cmake >nul 2>nul
if errorlevel 1 (
  call :find_cmake
  if errorlevel 1 exit /b 1
)
cmake --preset windows-x64
if errorlevel 1 goto failed
cmake --build --preset windows-release
if errorlevel 1 goto failed
ctest --preset windows-release
if errorlevel 1 goto failed
echo.
echo Compilation et tests termines.
echo Application : %CD%\build\windows-x64\Release\KravaKickLab.exe
pause
exit /b 0
:failed
echo.
echo Echec. Conservez le texte affiche pour le diagnostic.
pause
exit /b 1
:find_cmake
set "KRAVATEK_VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%KRAVATEK_VSWHERE%" goto missing_cmake
for /f "usebackq delims=" %%i in (`"%KRAVATEK_VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "KRAVATEK_VS=%%i"
set "KRAVATEK_CMAKE=%KRAVATEK_VS%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin"
if not exist "%KRAVATEK_CMAKE%\cmake.exe" goto missing_cmake
set "PATH=%KRAVATEK_CMAKE%;%PATH%"
exit /b 0
:missing_cmake
echo CMake ou Visual Studio C++ manque. Consultez README.md, rubrique Compilation Windows.
echo Aucune installation automatique n'a ete effectuee.
pause
exit /b 1
