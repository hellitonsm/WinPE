# WinPE

Scripts e utilitários para **criar, personalizar e testar imagens Windows PE 10**
(x86, x64, ARM e ARM64). O projeto resolve em especial o *loop de reboot* do WinPE,
mantendo o prompt de comando aberto, e permite injetar arquivos no `boot.wim`
direto na ISO — tudo sem reconstruir a imagem e sem perder a bootabilidade.

## Conteúdo

| Arquivo / pasta | Descrição |
|---|---|
| `WinPE_x64.bat`, `WinPE_x32.bat`, `WinPE_a64.bat`, `WinPE_a32.bat` | Criação da imagem no Windows com o **Windows ADK** (`copype` + `DISM`) e adição dos pacotes WinPE OC. |
| `WinPE_all.bat` | Executa os quatro scripts de criação em sequência. |
| `wpetool/` | Ferramenta Linux para inspecionar e editar o `boot.wim` dentro da ISO e ajustar o BCD. |
| `run-winpe-amd64.sh` | Cria/atualiza a VM `WinPE10-AMD64` no VirtualBox e inicializa com a ISO informada. |
| `fluxodebuild.md` | Fluxo resumido de montar → editar → compilar → gerar ISO → testar. |

## Requisitos

- **Windows:** Windows Assessment and Deployment Kit (ADK) com o complemento WinPE, para os `.bat`.
- **Linux:** `7z` (p7zip-full), `isoinfo` (genisoimage) e `python3`.
  As bibliotecas `wimlib` e `hivex` já vêm incluídas em `wpetool/lib/`, não é preciso `sudo`.
- **Teste:** VirtualBox com `VBoxManage` no `PATH`.

## Uso rápido

```bash
# Corrige o loop de reboot: adiciona winpeshl.ini que mantém o cmd.exe aberto
./wpetool/wpe-tool.sh fix-shell WinPE_amd64.iso WinPE_corrigida.iso

# Inspeciona a ISO gerada
./wpetool/wpe-tool.sh info WinPE_corrigida.iso

# Testa na VM
./run-winpe-amd64.sh WinPE_corrigida.iso
```

## Editando a imagem com uma pasta de trabalho

O fluxo abaixo é o equivalente ao `DISM /Mount-Image` + `/Unmount-Image /Commit`,
usando FUSE:

```bash
# 1. Monta o boot.wim descompactado em uma pasta
./wpetool/wpe-tool.sh workdir WinPE_amd64.iso wpe-work

# 2. Edita os arquivos normalmente
nano wpe-work/Windows/System32/startnet.cmd
cp meus_drivers/*.inf wpe-work/Windows/System32/

# 3. Gera a nova ISO (commit + recompressão + injeção)
./wpetool/wpe-tool.sh build wpe-work WinPE_editada.iso

# Descartar alterações: ./wpetool/wpe-tool.sh unmount wpe-work discard
```

O estado da montagem fica em `.wpetool-work/wpe-work/` e é removido
automaticamente pelo `build`/`unmount`.

## Menu "Desligar" incluído

`wpetool/templates/menu_iniciar.exe` é um pequeno programa Win32 com um botão
"Desligar" que chama `ExitWindowsEx(EWX_POWEROFF | EWX_FORCE, 0)`. O código-fonte
está em `wpetool/source/menu_iniciar.c`.

```bash
# Recompilar (MinGW-w64)
x86_64-w64-mingw32-gcc wpetool/source/menu_iniciar.c \
  -o wpetool/templates/menu_iniciar.exe -mwindows

# Montar, copiar os templates para System32 e gerar a ISO
./wpetool/wpe-tool.sh workdir WinPE_amd64.iso wpe-work
./wpetool/wpe-tool.sh settemplate wpe-work/Windows/System32
./wpetool/wpe-tool.sh build wpe-work WinPE_menu.iso
./run-winpe-amd64.sh WinPE_menu.iso
```

## Ajustes no BCD

```bash
./wpetool/wpe-tool.sh bcd-nocrashautoreboot original.iso saida.iso  # mostrar BSOD
./wpetool/wpe-tool.sh bcd-hv-off original.iso saida.iso             # desativar hypervisor
./wpetool/wpe-tool.sh bcd-serial-debug original.iso saida.iso       # debug na COM1
./wpetool/wpe-tool.sh bcd-patch original.iso saida.iso bootloader 26000024=01
```

Veja `wpetool/README.md` para a lista completa de comandos, elementos BCD e
detalhes de implementação.

## Observações sobre as ISOs

As imagens `.iso` **não são versionadas**: o `.gitignore` contém `*.iso`.
Elas são geradas localmente pelos scripts e podem ser baixadas do ambiente onde
o WinPE foi originalmente criado.

A substituição do `boot.wim` e do BCD é feita **in-place** na ISO, preservando o
layout ISO9660 e o offset gravado no diretório — reconstruir a ISO com
`genisoimage` altera o layout e o bootmgr não encontra o BCD. O WIM é recomprimido
em LZX para caber no espaço original.

## Estrutura do repositório

```text
.
├── WinPE_x64.bat / WinPE_x32.bat / WinPE_a64.bat / WinPE_a32.bat
├── WinPE_all.bat
├── run-winpe-amd64.sh
├── fluxodebuild.md
└── wpetool/
    ├── wpe-tool.sh
    ├── README.md
    ├── lib/          (wimlib, hivex e bibliotecas bundled)
    ├── source/       (menu_iniciar.c)
    └── templates/    (winpeshl.ini, startnet.cmd, menu_iniciar.exe)
```
