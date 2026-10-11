@echo off
cd /d "%~dp0"
where cmake >nul 2>nul || (echo Falta CMake. Instalalo desde cmake.org marcando Add to PATH. & pause & exit /b 1)
where git >nul 2>nul || (echo Falta Git. Instalalo desde git-scm.com. & pause & exit /b 1)
echo Compilando ArpGen, tarda unos minutos...
cmake -B build -G "Visual Studio 17 2022" -A x64 || goto fail
cmake --build build --config Release || goto fail
xcopy /E /I /Y "build\ArpGen_artefacts\Release\VST3\ArpGen.vst3" "C:\Program Files\Common Files\VST3\ArpGen.vst3" || goto fail
echo.
echo Listo. Abre FL Studio: Options, Manage plugins, Find installed plugins.
pause
exit /b 0
:fail
echo.
echo Algo fallo. Si el error es al copiar, ejecuta este archivo como administrador.
pause
exit /b 1
