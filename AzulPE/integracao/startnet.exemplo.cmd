@echo off
rem Modelo para um WinPE padrao. Preserve as linhas da sua imagem.
wpeinit
if exist "%SystemDrive%\AzulPE\AzulPE.exe" start "" /wait "%SystemDrive%\AzulPE\AzulPE.exe"
