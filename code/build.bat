@echo off
mkdir ..\build
pushd ..\build
cl ..\code\win32-terrain-generator.cpp ..\code\win32-opengl.cpp ..\code\maths.cpp ..\code\app.cpp ..\code\perlin.cpp ..\code\opengl-util.cpp ..\code\camera.cpp ..\code\imgui-master\*.cpp /MT /Zi user32.lib gdi32.lib opengl32.lib
popd

