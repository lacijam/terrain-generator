@echo off
mkdir ..\build
pushd ..\build
cl ..\code\win32-game.cpp ..\code\camera.cpp ..\code\matrix.cpp ..\code\opengl_loader.cpp ..\code\v3.cpp /Zi /D "_DEBUG" user32.lib gdi32.lib opengl32.lib
popd
