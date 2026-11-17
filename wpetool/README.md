# wpe-tool — edição de imagens Windows PE

Ferramenta para extrair, modificar e reinserir arquivos no `boot.wim` de uma ISO do
Windows PE, além de ajustar elementos do BCD. As dependências (wimlib e hivex) já vêm
bundadas em `lib/`, sem precisar de `sudo`.

## Uso rápido

```bash
cd /home/helliton/Downloads/WinPE-10.0.1904

# Verificar uma ISO / WIM
./wpetool/wpe-tool.sh info WinPE_amd64_v10.0.19041.1.iso

# Corrigir o loop de reboot (adiciona winpeshl.ini mantendo o cmd.exe aberto)
./wpetool/wpe-tool.sh fix-shell WinPE_amd64_v10.0.19041.1.iso WinPE_corrigida.iso
```

A ISO corrigida gerada por `fix-shell` foi exatamente a que resolveu o problema
desta pasta: o WinPE inicia e para no prompt `X:\windows\system32>`.

## Fluxo de pasta (recomendado)

É o equivalente ao `DISM /Mount-Image` + `/Unmount-Image /Commit`, usando FUSE:

```bash
# 1. Monta o boot.wim descompactado numa pasta
./wpetool/wpe-tool.sh workdir WinPE_amd64.iso wpe-work

# 2. EDITE A PASTA normalmente (arquivos reais, não dentro de um WIM)
nano wpe-work/Windows/System32/startnet.cmd
cp meus_drivers/*.inf wpe-work/Windows/System32/
echo "teste" > wpe-work/MEU_ARQUIVO.txt

# 3. Gera a nova ISO (commit + recompressão + injeção)
./wpetool/wpe-tool.sh build wpe-work WinPE_editada.iso
```

Para abandonar as alterações: `./wpetool/wpe-tool.sh unmount wpe-work discard`.
O estado da montagem fica em `.wpetool-work/wpe-work/` (ao lado da pasta) e é
removido automaticamente pelo `build`/`unmount`.

> **Importante:** o `build` recomprime o WIM em LZX para ele caber no espaço
> original da ISO e preenche o restante com zeros. O ISO continua bootável porque
> o tamanho gravado no diretório ISO9660 é mantido.

## Edição direta no WIM (sem montar)

```bash
./wpetool/wpe-tool.sh extract WinPE_amd64.iso trabalho
./wpetool/wpe-tool.sh list trabalho/boot.wim
./wpetool/wpe-tool.sh wim-add trabalho/boot.wim ./meu.cmd /Windows/System32/startnet.cmd
./wpetool/wpe-tool.sh wim-delete trabalho/boot.wim /Windows/System32/algum.ini
./wpetool/wpe-tool.sh wim-optimize trabalho/boot.wim
./wpetool/wpe-tool.sh inject WinPE_amd64.iso trabalho/boot.wim nova.iso
```

## Programa exemplo: menu_iniciar.exe

O código-fonte está em `wpetool/templates/menu_iniciar.c`. É uma janela Win32 com um
único botão "Desligar" que chama `ExitWindowsEx(EWX_POWEROFF | EWX_FORCE, 0)`.

### Compilar no Windows

### Opção 1: Visual Studio (recomendado)
1. Abra o Visual Studio.\n2. Crie um novo projeto "Console Application" ou "Win32 Project".\n3. Cole o código-fonte em `menu_iniciar.c`.\n4. No projeto, defina o tipo de saída como "Windows Application" (não console).\n5. Compile (Ctrl+F7). O executável `menu_iniciar.exe` será gerado.\n\n### Opção 2: MinGW-w64 (linha de comando)
1. Instale o MinGW-w64 (https://www.mingw-w64.org).\n2. Abra o terminal "MSYS2 MinGW 64-bit".\n3. Execute:\n   ```bash\n   x86_64-w64-mingw32-gcc -mwindows menu_iniciar.c -o menu_iniciar.exe\n   ```\n\n### Opção 3: ADK / Windows SDK\n1. Instale o Windows Assessment and Deployment Kit (ADK).\n2. No Visual Studio, crie um projeto "Win32 Project".\n3. Defina o modelo como "Windows Application".\n4. Compile.\n\n> **Importante:** o executável deve ter o ícone padrão do Windows ou ser simples;\n> o WinPE tem poucos recursos, então mantenha o aplicativo leve.\n\n> **Importante:** o executável deve ter o ícone padrão do Windows ou ser simples;\n> o WinPE tem poucos recursos, então mantenha o aplicativo leve.\n\n## Como integrar ao WinPE\n\n### Via fluxo de pasta (recomendado)\n```bash\n# 1. Monta o boot.wim descompactado numa pasta\n./wpetool/wpe-tool.sh workdir WinPE_amd64.iso wpe-work\n\n# 2. Copie o executável para a pasta montada\ncp menu_iniciar.exe wpe-work/Windows/System32/\n\n# 3. (Opcional) edite startnet.cmd se quiser lançar o menu automaticamente\nnano wpe-work/Windows/System32/startnet.cmd\n\n# 4. Gera a nova ISO\n./wpetool/wpe-tool.sh build wpe-work WinPE_editada.iso\n```\n\n### Via edição direta no WIM (sem montar)\n```bash\n./wpetool/wpe-tool.sh extract WinPE_amd64.iso trabalho\n./wpetool/wpe-tool.sh wim-add trabalho/boot.wim ./menu_iniciar.exe /Windows/System32/menu_iniciar.exe\n./wpetool/wpe-tool.sh wim-optimize trabalho/boot.wim\n./wpetool/wpe-tool.sh inject WinPE_amd64.iso trabalho/boot.wim nova.iso\n```\n\n## Teste final\n\n1. Gere a ISO nova:\n   ```bash\n   ./wpetool/wpe-tool.sh build wpe-work WinPE_novo.iso\n   ```\n2. Substitua a ISO no VirtualBox:\n   ```bash\n   ./run-winpe-amd64.sh WinPE_novo.iso\n   ```\n3. A VM deve abrir com a janela \"Menu Iniciar\" e, ao clicar no botão \"Desligar\",\n   o sistema deve desligar sem reiniciar automaticamente.\n\nSe tudo funcionar, o problema do loop de reboot foi resolvido e o fluxo de\nmodificação do WinPE está validado.\n\n## BCD (opções avançadas)

```bash
# Evitar reboot automático em BSOD (mostra o código de erro)
./wpetool/wpe-tool.sh bcd-nocrashautoreboot original.iso saida.iso

# Desativar hypervisorlaunchtype
./wpetool/wpe-tool.sh bcd-hv-off original.iso saida.iso

# Ativar debug do kernel na COM1 (115200, 8N1)
./wpetool/wpe-tool.sh bcd-serial-debug original.iso saida.iso

# Elemento específico: bootloader 26000024 (nocrashautoreboot) = 01
./wpetool/wpe-tool.sh bcd-patch original.iso saida.iso bootloader 26000024=01
```

Elementos BCD úteis (entrada `bootloader`):

| Elemento | Nome | Valores |
|---|---|---|
| `26000024` | nocrashautoreboot | `01` (1 byte) |
| `26000022` | winpe | `01` |
| `15000011` | debugtype | `00` = serial (8 bytes LE) |
| `15000013` | debugport | `01` = COM1 (8 bytes LE) |
| `15000014` | baudrate | `00c2010000000000` = 115200 |
| `16000010` | bootdebug | `01` (1 byte) |

## Como funciona

- `7z` extrai o `sources/boot.wim` da ISO.
- `wimlib-imagex update` adiciona/remove arquivos no WIM;
  `wimlib-imagex optimize --recompress --compress=lzx` o reduz.
- `iso_wim.py` lê a entrada do diretório ISO9660 para achar o offset exato do
  `boot.wim`; `iso_patch_bytes.py` o substitui sem alterar mais nada na ISO.
- `hivedit.py` usa `hivex` para editar o BCD (também substituído in-place nas
  duas cópias: BIOS e EFI).
- Rebuild completo com `genisoimage` **não** foi usado: ele muda o layout da ISO
  e o bootmgr não encontra o BCD. A substituição in-place é segura.
