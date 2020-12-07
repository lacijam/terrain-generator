@echo off
mkdir ..\build
pushd ..\build
cl ..\code\win32-terrain-generator.cpp /Zi /D "_DEBUG" user32.lib gdi32.lib opengl32.lib
popd

