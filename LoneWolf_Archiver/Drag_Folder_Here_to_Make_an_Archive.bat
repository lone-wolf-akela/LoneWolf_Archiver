@echo off
cd /d "%~dp0"
LoneWolf_Archiver -r %1 -a %~n1.big -g
pause