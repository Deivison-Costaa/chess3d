@echo off
:: Adiciona MinGW ao PATH para resolver libstdc++, libgcc, libwinpthread
set PATH=C:\msys64\mingw64\bin;%PATH%

set EXE=%~dp0build\bin\chess3d.exe

if not exist "%EXE%" (
    echo [ERRO] Binario nao encontrado: %EXE%
    echo Execute o build primeiro:
    echo   cmake -B build -S . --preset windows-mingw
    echo   cmake --build build
    pause
    exit /b 1
)

"%EXE%" %*
