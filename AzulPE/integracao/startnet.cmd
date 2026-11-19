@echo off
rem Inicializa o WinPE (rede, drivers, variáveis de ambiente)
wpeinit
rem Lança o Azul PE a partir da raiz da unidade do sistema (X: em WinPE de RAM)
if exist "%SystemDrive%\AzulPE\AzulPE.exe" (
    start "" /wait "%SystemDrive%\AzulPE\AzulPE.exe"
) else (
    echo AzulPE.exe nao encontrado em %SystemDrive%\AzulPE\
    echo Copie AzulPE.exe para X:\AzulPE\ antes de iniciar o WinPE.
)
