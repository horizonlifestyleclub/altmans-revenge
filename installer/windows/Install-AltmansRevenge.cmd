@echo off
title Altman's Revenge - installer
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0install.ps1" %*
pause
