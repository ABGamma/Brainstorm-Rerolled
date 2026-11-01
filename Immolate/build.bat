@echo off
rem Builds Immolate.dll and runs the tests (Windows, Visual Studio 2022).
rem Output: build\Release\Immolate.dll  - copy it into your Brainstorm mod folder.
cd /d "%~dp0"
cmake -G "Visual Studio 17 2022" -A x64 -B build || goto :error
cmake --build build --config Release --parallel || goto :error
ctest --test-dir build -C Release --output-on-failure || goto :error
pause
exit /b 0
:error
echo Build failed.
pause
exit /b 1
