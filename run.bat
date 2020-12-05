@echo off
if "%1"=="D" (
    goto debugger
) else (
    powershell -Command "Measure-Command {./build.bat %1 | Out-Default} | select @{n=\" \"; e={\"Build time:\",$_.Minutes,\"m\",$_.Seconds,\"s\",$_.Milliseconds,\"ms\" -join \" \"}}"
    if "%2"=="D" (
        goto debugger
    )
)

exit

:debugger
if not defined DevEnvDir (
    call "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvarsall.bat" x64
)
devenv /DebugExe opengl.exe
