@echo off
powershell -Command "Measure-Command {./_build_script.bat %1 | Out-Default} | select @{n=\" \"; e={\"Build time:\",$_.Minutes,\"m\",$_.Seconds,\"s\",$_.Milliseconds,\"ms\" -join \" \"}}"
