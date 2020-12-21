@echo off
mkdir ..\build
pushd ..\build
cl ..\code\win32-terrain-generator.cpp ..\code\win32-opengl.cpp ..\code\maths.cpp ..\code\app.cpp ..\code\perlin.cpp ..\code\opengl-util.cpp /Zi /D "_DEBUG" user32.lib gdi32.lib opengl32.lib
popd

