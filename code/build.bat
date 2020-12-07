@echo off
mkdir ..\build
pushd ..\build
cl ..\code\terrain-generator.cpp ..\code\win32-window.cpp ..\code\camera.cpp ..\code\matrix.cpp ..\code\opengl.cpp  ..\code\v3.cpp /Zi /D "_DEBUG" user32.lib gdi32.lib opengl32.lib
popd

