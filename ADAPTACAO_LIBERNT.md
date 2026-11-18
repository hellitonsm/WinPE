# Adaptação do `explorer.exe` do LiberNT para rodar no WinPE

Documento de trabalho: lista **todas** as mudanças necessárias para fazer o
explorer do LiberNT (fork do ReactOS) inicializar dentro da imagem WinPE do
projeto, com o estado atual das tentativas e dos bloqueios.

> Status: **não concluído**. O shell do WinPE hoje é o `wpetool/source/explorer.c`
> (Windows 11-like), que roda. O explorer real do LiberNT ainda não inicializa.

---

## 1. O que está funcionando hoje

| Item | Estado |
|---|---|
| Shell customizado (`wpetool/source/explorer.c`) | ✅ Boot no WinPE, taskbar, menu, explorador |
| ISO `WinPE_libernt_look.iso` | ✅ Gerada e testada na VM |
| `wpetool/templates/explorer.exe` | ✅ Compilado com MinGW |
| Debug no LiberNT explorer | ✅ Funcional (janela de log + SEH) |
| `lnt_launch.c` (launcher anti-reboot) | ✅ Mostra o erro e abre `cmd.exe` |
| `lnt_probe.c` (probe de LoadLibrary) | ✅ Mostra erro na tela |
| Explorador real do LiberNT no WinPE | ❌ Não sobe (veja §5) |

---

## 2. Mudanças de build no LiberNT

SDL de trabalho: `output-Clang-amd64-debug/` (cmake + ninja).

### 2.1 DLLs compiladas por nós (não estavam no output)

```bash
cd LiberNT
CCACHE_DISABLE=1 ninja -C output-Clang-amd64-debug \
    dll/win32/shlwapi/shlwapi.dll
CCACHE_DISABLE=1 ninja -C output-Clang-amd64-debug \
    dll/win32/windows.storage/windows.storage.dll
```

Resultado:
- `output-Clang-amd64-debug/dll/win32/shlwapi/shlwapi.dll` (645 KB)
- `output-Clang-amd64-debug/dll/win32/windows.storage/windows.storage.dll` (1,3 MB)

### 2.2 Patch manual de binários (bundle `/tmp/opencode/lnt-bundle/`)

`comctl32.dll`: o import `kernelbase_ros.dll` (ReactOS-only, nunca existirá no
WinPE) foi trocado em bytes por `kernel32.dll` (mesmo conjunto de funções).

`uxtheme.dll`: três imports que a `user32.dll` da Microsoft **não** exporta
foram redirecionados para equivalentes existentes:

| Import original | Trocado por | Motivo |
|---|---|---|
| `KillSystemTimer` | `KillTimer` | Assinatura idêntica |
| `SetSystemTimer` | `SetTimer` | Assinatura idêntica |
| `UserSetLightColorScheme` | `SetFocus` | Código ReactOS vira no-op inofensivo |

> Observação: os **forwarders** do `comctl32.dll` para `kernelbase_*` foram
> checados e resolvem contra a `kernelbase.dll` da Microsoft (2019 exports,
> nenhum faltando). Os imports por **nome** de todas as DLLs bundladas foram
> checados estaticamente contra as exports das DLLs do WinPE e do bundle.

### 2.3 Bundle montado

Cópia em `X:\LiberNT\` na imagem do WinPE:

```
explorer.exe          (LiberNT, recompilado com debug)
shell32.dll           (ReactOS, 4,5 MB)
shlwapi.dll           (ReactOS, 645 KB)
comctl32.dll          (ReactOS, 1,4 MB, patch kernelbase_ros→kernel32)
explorerframe.dll     (ReactOS, 2,1 MB)
gdiplus.dll           (ReactOS, 712 KB)
uxtheme.dll           (ReactOS, 660 KB, imports user32 patchados)
shdocvw.dll           (ReactOS, 269 KB)
browseui.dll          (ReactOS, 6 KB, forwarder puro)
windows.storage.dll   (ReactOS, 1,3 MB)
lnt_launch.exe        (launcher de debug, MinGW)
lnt_probe.exe         (probe de DLLs, MinGW)
```

---

## 3. Mudanças no registry da imagem WinPE

Hives editados com `hivex` (via `wpetool/lib/python/hivex` + `LD_LIBRARY_PATH=wpetool/lib/lib`).

### 3.1 `SYSTEM` — KnownDLLs

Remover as entradas que forçavam as DLLs da Microsoft, permitindo que o
bundle carregue as versões ReactOS:

```
SHELL32
SHLWAPI
gdiplus
COMDLG32
SHCORE
```

**Importante:** `KERNEL32`, `USER32`, `GDI32`, `ADVAPI32`, `OLE32`,
`OLEAUT32`, `MSVCRT`, `Setupapi`, `ADVAPI32` etc. continuam KnownDLLs — o
explorer do LiberNT não pode operar sem a semântica do Windows nelas.

### 3.2 `SOFTWARE` — Winlogon

`HKLM\Software\Microsoft\Windows NT\CurrentVersion\Winlogon`

```
Shell = REG_SZ "explorer.exe"
```

Sem isso `IsExplorerSystemShell()` (`explorer.cpp`) retorna `FALSE` e o
explorer cai no caminho `StartWithCommandLine()`, que abre uma janela de
comando em vez de criar desktop/taskbar.

### 3.3 `DEFAULT` — perfil inicial (HKCU do SYSTEM/.DEFAULT)

Criar:

```
HKCU\Software\Microsoft\Windows\CurrentVersion\Explorer
```

Sem essa chave, `Explorer_Main → StartWithDesktop` abre um
`MessageBox(IDS_STARTUP_ERROR)` e **retorna 1 antes de qualquer janela**.

### 3.4 Notas
- Em vez de substituir `System32\shell32.dll`/`shlwapi.dll`, o caminho correto
  é remover os KnownDLLs (substituição causou **BSOD `0x21A`** — o winlogon/csr
  do WinPE não tolera a shell32 do ReactOS).
- O hive `DEFAULT` do WinPE é o HKCU do usuário SYSTEM; a seção
  `Run`/`RunOnce`/`StartupApproved`/`ShellServiceObjectDelayLoad` usada por
  `startup.cpp` continua vazia/inexistente.

---

## 4. Mudanças no `winpeshl.ini`

Versão de produção (nunca chegou lá):

```ini
[LaunchApps]
%SystemRoot%\System32\explorer.exe
```

Versão de debug usada nos testes:

```ini
[LaunchApps]
X:\LiberNT\lnt_launch.exe
```

---

## 5. Estado atual dos testes (na VM headless `WinPE10-AMD64`)

Sequência cronológica e causa):

| Tentativa | Exit code | Causa identificada |
|---|---|---|
| Bundle app-local, KnownDLLs originais | `0xC0000139` | KnownDLLs carregavam `shell32.dll`/`shlwapi.dll` da Microsoft → ordinal ReactOS privado inexistente |
| Substituir `System32\shell32.dll` + `shlwapi.dll` | BSOD `0x21A` | `csrss.exe`/`winlogon` do WinPE não sobreviveram à shell32 do ReactOS |
| Bundle + remover KnownDLLs | `0xC0000135` | Módulo não encontrado (dependência transitiva) |
| Probe (`LoadLibrary` 1 a 1) | ver abaixo | `comctl32` falha com `127`; `explorerframe`/`shdocvw`/`windows.storage` com `126` |

### 5.1 Diagnóstico do probe (tela da VM)

```
FAIL shell32.dll         err=0x86
OK   shlwapi.dll
FAIL comctl32.dll        err=0x7F  (ERROR_PROC_NOT_FOUND)
FAIL explorerframe.dll   err=0x7E  (ERROR_MOD_NOT_FOUND)
OK   gdiplus.dll
OK   uxtheme.dll
FAIL shdocvw.dll         err=0x7E
OK   browseui.dll
FAIL windows.storage.dll err=0x7E
```

- `explorerframe`, `shdocvw` e `windows.storage` falham porque **dependem de `comctl32`**
  (transitivo). Consertar o `comctl32` provavelmente desbloqueia os três.
- A causa exata do `127` no `comctl32` **não foi fechada**. Os imports por nome
  contra `user32/kernel32/gdi32/advapi32/ntdll` da Microsoft estão todos
  presentes; os forwarders para `kernelbase` resolvem (2019 exports); os 3
  imports que faltavam na `user32` já foram patcheados. Hipóteses abertas:
  1. CRT do clang (`msvcrt`→`ucrtbase`): algum forwarder do `msvcrt` que o
     `comctl32` usa não existe no `ucrtbase` do WinPE.
  2. `comctl32` tem dependência de `kernelbase_ros` em mais um caminho não
     coberto pelo patch (variável, string de forwarder ou export ordinal).
  3. Assemblies SxS de `comctl32`/`explorerframe` (há `dependentAssembly` nos
     manifests embarcados; os v6 existem em `WinSxS/amd64_..._6.0.26100.1_...`).

---

## 6. Mudanças ainda necessárias para completar a adaptação

### 6.1 Desbloquear `comctl32.dll` (bloqueador do momento)

- [ ] Identificar import exato que dá `127` (runtime probe com
      `LoadLibraryEx(..., DONT_RESOLVE_DLL_REFERENCES)` + enumeração manual
      da tabela de imports, ou `ShowLoaderSnaps` no hive SYSTEM para ver o
      log do loader via `OutputDebugString`/debugger de kernel).
- [ ] Corrigir (patch de string no binário, ou recompilar `comctl32` com
      `kernelbase_ros` realmente resolvido — indisponível no WinPE; melhor é
      eliminar os usos).
- [ ] Revalidar `explorerframe`, `shdocvw`, `windows.storage` (devem passar a
      carregar).

### 6.2 Conferir `windows.storage.dll`

- [ ] Resolver `combase.dll` (presente no WinPE em `System32`, mas nunca foi
      carregado no probe) e quaisquer outros imports ReactOS-only.
- [ ] Decidir entre manter bundle ou linkar no `explorer.exe`.

### 6.3 Registry / perfil / COM

- [ ] Garantir `HKCU\...\Explorer` e `HKCU\...\Explorer\Advanced` (valores
      `IconsOnly`, `ShowInfoTip` etc. usados por `taskbarsettings.cpp`).
- [ ] `HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\Run` / `RunOnce` /
      `StartupApproved` — o `startup.cpp` lê todos; o WinPE não tem nenhum,
      o que é seguro (no-op), mas `RUNONCE` pode travar sem `__fastload` se
      a chave existir semi-criada.
- [ ] Registrar os CLSIDs do shell usados pelo explorer:
      - `CLSID_StartMenu`, `CLSID_BandSiteMenu`, `CLSID_RebarBandSite`,
        `CLSID_MenuBand` (criação via `CoCreateInstance` em `traywnd.cpp`,
        `rshell.cpp`);
      - objetos de notificação (rede/volume/falante): `netshell`,
        `stobject`, `TrayNotificationArea` — o WinPE tem `netshell.dll`,
        mas os CLSIDs ReactOS esperados não existem.
- [ ] Verificar `HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\ShellServiceObjectDelayLoad`
      (`shellservice.cpp`) — WinPE provavelmente não tem.
- [ ] `msgina: ShellReadyEvent` é um evento XP; o WinPE não tem msgina. O
      `explorer.cpp` tenta `SetShellReadyEvent` mas **tolera falha** — ok.
- [ ] `ms-settings:*` URIs usados pelo relógio e flyouts não existem no WinPE —
      checar se `trayclock.cpp`/`trayflyouts.cpp` degradam ou crasheiam.

### 6.4 Boot / shell decision

- [ ] `GetShellWindow() == NULL` no WinPE (sem shell) → `bExplorerIsShell = TRUE`
  quando `IsExplorerSystemShell()` lê `Shell = explorer.exe` do SYSTEM —
  confirmado como correto.
- [ ] `Winpeshl` deve **não** ser o `cmd.exe` do fluxo atual; o
  `winpeshl.ini` deve apontar para o launcher de debug durante o
  desenvolvimento, e para o próprio explorer em produção.

### 6.5 Estabilidade dentro do WinPE

- [ ] `dwm`/`Taskbar::Liquid` — o WinPE não tem DWM; `ShellOpenTaskbarMaterial()`
      retorna nulo, `IsThemeActive()` pode retornar FALSE. Conferir se os
      caminhos de código degradam (`traywnd.cpp`, `trayflyouts.cpp`,
      `startmnu2.cpp`).
- [ ] Ícones Fluent são carregados via `LoadIcon`/`LoadImage` de recursos —
      OK, não dependem de rede.
- [ ] `GetAdaptersAddresses` (`iphlpapi`) — WinPE tem `IPHLPAPI.DLL`; pode
      retornar zero adaptadores — ok, código lida com isso?
- [ ] `mixerOpen`/`mixerGetLineInfoW` (`winmm`) para o ícone de volume —
      provavelmente retorna erro; conferir tratamento.
- [ ] `CM_*` / `SetupDi*` (`setupapi`) para detecção de dock/eject — WinPE
      tem `setupapi.dll`; confirmar que `CM_Is_Dock_Station_Present` existe.
- [ ] `powrprof.dll` — confirmar `GetPwrCapabilities` no WinPE (pode não
      existir; usado pelo menu de energia).

### 6.6 Qualidade do binário

- [ ] **CRT do clang vs MS x64**: `_vsnwprintf`, `_beginthreadex`, etc. O
      binário do LiberNT foi linkado contra `msvcrt.dll` + `ucrtbase.dll` —
      o WinPE tem ambos, mas a família `_wcsicmp` pode não existir na
      `msvcrt` da Microsoft (foi SUPORTA como forwarder para `ucrtbase`?
      conferir).
- [ ] Recompilar `explorer.exe` com `-O0` + `/GS` desabilitado ou com
      `-fno-stack-protector` se suspeitas de SSP; não é requisito.
- [ ] Rodar `cppcheck` (ver `AGENTS.md`) sobre o diff após remover o debug.

### 6.7 Alternativa "estática" propriamente dita

- [ ] Reescrever as DLLs ReactOS como `STATIC` no CMake (shell32, shlwapi,
      comctl32, explorerframe, gdiplus, uxtheme, shdocvw, browseui,
      windows.storage) e linká-las no `explorer.exe`. Isso elimina o
      problema de KnownDLLs e dispensa o registro de CLSIDs? **Não** — os
      servidores COM continuam sendo DLL servers e precisam de
      `CoRegisterClassObject` próprio. Estimativa: 2–4 semanas de build,
      alto risco de colisão de símbolos (`IShell*`, `DllRegisterServer`).
- [ ] Mesmo com tudo estático, falta perfil/registry/COM — ver §6.3.
- [ ] Se quiser usar o binário pronto sem compilar nada: usar
      `lnt_launch.exe`/`lnt_probe.exe` como empacotadores de dependências
      (funciona, mas é o mesmo caminho com outro nome).

---

## 7. Verificações finais necessárias

- [ ] Boot até a área de trabalho com taskbar visível.
- [ ] Menu Iniciar abre, busca funciona, tiles lançam.
- [ ] Explorador de arquivos abre em `shell:MyComputerFolder` e navega.
- [ ] Desligar/reiniciar via `wpeutil.exe` funciona (não via `ExitWindowsEx`,
      que reboota no WinPE).
- [ ] Nenhum BSOD; se acontecer, capturar com `verifier`/dump (WinPE tem
      `pmem`? `explorer` 32-bit? não aplicável neste caso).
- [ ] Testar em **outra** resolução/GPU além de `1024x768 vmsvga`.

---

## 8. Riscos e alternativas

| Opção | Risco | Custo | Resultado |
|---|---|---|---|
| Bundle app-local + remover KnownDLLs (atual) | `comctl32` n carrega → `explorerframe`/`shdocvw`/`windows.storage` também falham | médio | parcial |
| Substituir `shell32` do System32 | **BSOD 0x21A** — WinPE não sobrevive | alto | inutilizável |
| Link estático de todos os módulos ReactOS | semanas de build, colisões de COM/CLSID | muito alto | teórico |
| **Manter o `explorer.c` próprio (hoje)** | nenhum | baixo | **funciona** |
| Rodar o LiberNT como SO real (QEMU/VM) | nenhum | build completo do LiberNT | explorer nativo |

---

## 9. Artefatos e locais

| Caminho | Conteúdo |
|---|---|
| `wpetool/source/explorer.c` | Shell customizado (funcional) |
| `wpetool/source/lnt_launch.c` | Launcher anti-reboot com código de saída |
| `wpetool/source/lnt_probe.c` | Probe de LoadLibrary |
| `wpetool/templates/explorer.exe` | Shell customizado compilado |
| `LiberNT/base/shell/explorer/explorer.cpp` | Explorer do LiberNT **com** janela de debug |
| `output-Clang-amd64-debug/base/shell/explorer/explorer.exe` | Explorer do LiberNT recompilado |
| `/tmp/opencode/lnt-bundle/` | Bundle ReactOS (session storage) |
| `WinPE_libernt_debug3.iso` | ISO com launcher + debug (não sobe) |
| `WinPE_libernt_look.iso` | ISO funcional com o shell customizado |
| `/tmp/opencode/libernt_explorer_debug.log` | Log da inicialização (no X: do WinPE) |

---

## 10. Comandos para reproduzir

```bash
# 1. Build das DLLs faltantes
cd LiberNT
CCACHE_DISABLE=1 ninja -C output-Clang-amd64-debug dll/win32/shlwapi/shlwapi.dll
CCACHE_DISABLE=1 ninja -C output-Clang-amd64-debug dll/win32/windows.storage/windows.storage.dll

# 2. Bundle
mkdir /tmp/opencode/lnt-bundle
cp output-Clang-amd64-debug/base/shell/explorer/explorer.exe /tmp/opencode/lnt-bundle/
for f in shell32 shlwapi comctl32 explorerframe gdiplus uxtheme shdocvw browseui; do
  src=$(find output-Clang-amd64-debug -name "$f.dll" ! -path '*/symbols/*' | head -1)
  cp "$src" /tmp/opencode/lnt-bundle/$f.dll
done
cp output-Clang-amd64-debug/dll/win32/windows.storage/windows.storage.dll /tmp/opencode/lnt-bundle/

# 3. Patch dos binários (ver §2.2)
# ... python3 com patch de bytes

# 4. Patch dos hives (ver §3) e do winpeshl.ini (ver §4)
# ... hivex em SYSTEM/SOFTWARE/DEFAULT

# 5. Build da ISO
./wpetool/wpe-tool.sh workdir WinPE_amd64.iso wpe-lnt
# copiar bundle, hives patchados, winpeshl.ini
./wpetool/wpe-tool.sh build wpe-lnt WinPE_libernt_debug3.iso

# 6. Teste
./run-winpe-amd64.sh WinPE_libernt_debug3.iso
```
