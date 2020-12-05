@echo off
set VC_VERSION=2019
set APP_NAME=opengl
set PROJECT_PATH=%~dp0
set SRC_PATH=\src
setlocal EnableDelayedExpansion

if "%1" == "d" (
    if exist "%APP_NAME%.pdb" (
        del "%APP_NAME%.pdb" /q
    )

    if not exist "build" (
        mkdir build
    )

    pushd build

    if not exist "%APP_NAME%" (
        mkdir %APP_NAME%
    )
        
    pushd %APP_NAME%

    if not defined DevEnvDir (
        call "C:\Program Files (x86)\Microsoft Visual Studio\%VC_VERSION%\Community\VC\Auxiliary\Build\vcvarsall.bat" x64
    )

    cl /c %PROJECT_PATH%\%SRC_PATH%\%APP_NAME%\*.cpp /ZI /Od /W3 /MDd /EHsc /D "_DEBUG" /D "_UNICODE" /D "UNICODE"
    
    if !errorlevel! equ 0 (
        echo Compile success, starting linker...

        link *.obj kernel32.lib user32.lib gdi32.lib opengl32.lib /OUT:%APP_NAME%.exe /DEBUG:FASTLINK /INCREMENTAL /MACHINE:X64 /SUBSYSTEM:WINDOWS
        if !errorlevel! equ 0 (
            goto cleanup
        )
    )

    goto fail
) else (
    if "%1"=="r" (
        if exist "%APP_NAME%.pdb" (
            del "%APP_NAME%.pdb" /q
        )

        if not exist "build" (
            mkdir build
        )

        pushd build

        if not exist "%APP_NAME%" (
            mkdir %APP_NAME%
        )
        
        pushd %APP_NAME%

        if not defined DevEnvDir (
            call "C:\Program Files (x86)\Microsoft Visual Studio\%VC_VERSION%\Community\VC\Auxiliary\Build\vcvarsall.bat" x64
        )

        cl /c %PROJECT_PATH%\%SRC_PATH%\%APP_NAME%\*.cpp /Zi /O2 /Oi /GL /MD /W3 /EHsc /D "_NDEBUG" /D "_UNICODE" /D "UNICODE"
        
        if !errorlevel! equ 0 (
            echo Compile success, starting linker...

            link kernel32.lib user32.lib gdi32.lib opengl32.lib /OUT:%APP_NAME%.exe /DEBUG:FULL /INCREMENTAL:NO /OPT:REF /OPT:ICF /MACHINE:X64 /SUBSYSTEM:WINDOWS

            if !errorlevel! equ 0 (
                goto cleanup
            )
        )
    )
)

:cleanup
echo =================
echo Successful Build!
echo =================
echo Copying executable and pdb to %PROJECT_PATH%
copy /y .\%APP_NAME%.exe %PROJECT_PATH%
copy /y .\%APP_NAME%.pdb %PROJECT_PATH%
popd
popd

goto end

:fail
echo ================
echo Failed Build!
echo ================

:end
echo Exiting...
exit /b ERRORLEVEL

