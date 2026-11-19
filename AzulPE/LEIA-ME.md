# Azul PE 1.0

Shell gráfico portátil em **português do Brasil**, para **Windows PE x64 (amd64)** baseado no Windows 10/11. O visual foi inspirado na referência: papel de parede azul com curvas de luz, ícones à esquerda, barra escura e menu Iniciar de duas colunas. A interface e a arte estão incorporadas em um único executável C++/Win32. O programa não exige `explorer.exe`, .NET, PowerShell ou conexão com a Internet.

A captura `Previa.png` mostra o executável em execução no Wine com uma unidade e pastas de teste.

## Executar sem instalar

1. Extraia o ZIP. Para uso portátil, basta copiar **`AzulPE.exe`** para o pendrive ou para a imagem WinPE.
2. No prompt do WinPE, execute o arquivo da unidade em que ele está. Por exemplo:

   ```cmd
   E:\AzulPE\AzulPE.exe
   ```

   Troque `E:` pela letra do seu pendrive. `diskpart` → `list volume` → `exit` permite conferir as letras sem alterá-las.
3. Clique em **Iniciar** ou dê duplo clique em **Este PC**. Para sair sem desligar o computador, escolha **Iniciar → Encerrar** (fora do WinPE) ou **Iniciar → seta ao lado de Desligar → Sair do Azul PE**.

É possível testar a interface no Windows 10/11 x64 comum. Ela abre em tela cheia na tela principal e não substitui o shell do Windows, não pede elevação e não muda o registro nem o seu papel de parede. O executável não está assinado digitalmente; o Windows pode mostrar um aviso ao abri-lo.

### Opções de linha de comando

| Comando | Efeito |
|---|---|
| `AzulPE.exe` | Mostra a área de trabalho. |
| `AzulPE.exe --explorer` | Abre o Explorador Azul em **Este PC**. |
| `AzulPE.exe --caminho "E:\Meus arquivos"` | Abre o Explorador Azul na pasta indicada. |
| `AzulPE.exe --inicio` | Abre o menu Iniciar depois da inicialização. Pode ser combinado com `--explorer`. |

Apenas uma instância é mantida por sessão. Se o programa já estiver aberto, executá-lo novamente traz o menu Iniciar.

## O que está implementado

- **Área de trabalho:** atalhos para Documentos, Este PC, Rede e Prompt de Comando; barra de tarefas com relógio, calendário no formato dia/mês/ano e botão no extremo direito para mostrar a área de trabalho. A barra controla o Explorador Azul e o leitor de texto; aplicativos externos não ganham botões individuais.
- **Menu Iniciar:** atalhos pesquisáveis, grupos expansíveis e acesso aos locais da sessão. Prompt, Rede (`ipconfig`), Bloco de Notas e Gerenciador de Tarefas usam as ferramentas que a imagem disponibilizar. Desligar e reiniciar só são autorizados quando o programa detecta WinPE e encontra `wpeutil.exe`; pedem confirmação.
- **Explorador Azul:** unidades montadas e espaço livre quando disponível; navegação por pastas, histórico, níveis acima e endereços locais/UNC; filtro na pasta atual; lista com nome, data, tipo e tamanho; criação de pasta, cópia de arquivo/pasta, colagem, renomeação e exclusão com confirmação. Texto comum abre no leitor nativo de somente leitura. Outros arquivos usam a associação da imagem, quando houver.

A seleção é de **um item por vez**. Copiar/colar itens funciona dentro do Azul PE, sem sobrescrever o destino e sem seguir links/junções. A cópia pode ser cancelada na barra de status. **Excluir** não usa Lixeira: só remove arquivos e pastas vazias, com confirmação; não há remoção recursiva. A listagem mostra os itens ocultos e de sistema, úteis em recuperação. Não há indexação de disco ou desbloqueio automático de BitLocker. Rede, unidades e acesso aos dados ainda dependem dos drivers, permissões e da configuração do WinPE. A unidade do sistema em um WinPE de RAM costuma ser `X:`; alterações ali podem se perder ao reiniciar.

### Teclado

| Atalho | Efeito |
|---|---|
| `Ctrl+Esc` | Abre/fecha o Iniciar quando o Azul PE está ativo. |
| `Esc` / `↑` `↓` / `Enter` | Fecha o Iniciar, percorre ou abre atalhos. |
| `Ctrl+L` / `Ctrl+F` | Edita o endereço / filtra a lista no Explorador. |
| `Alt+←` / `Alt+→` / `Alt+↑` | Volta, avança ou sobe uma pasta. |
| `F5` / `F6` | Atualiza unidades e lista / alterna o foco do Explorador. |
| `Ctrl+C` / `Ctrl+V` | Copia e cola o item selecionado no Explorador. |
| `Ctrl+Shift+N` / `F2` / `Del` | Cria pasta, renomeia ou pede confirmação para excluir. |

## Iniciar junto com o WinPE

**Faça uma cópia da imagem e teste primeiramente em VM.** Os comandos abaixo são para um `boot.wim` **WinPE amd64** criado com o ADK. Não rode os comandos na imagem que você pretende manter como única cópia. Use um Prompt de Comando **administrador no Windows de manutenção**, não dentro do WinPE em execução. Se você usar o `gerar-imagem-azul.sh` da seção "Integração automatizada" (Linux), o Azul PE já vem instalado como shell padrão e estes passos manuais não são necessários.

1. Verifique o índice da sua imagem; nas mídias simples criadas pelo ADK costuma ser `1`. Substitua os caminhos de exemplo pelos seus e monte a imagem em uma pasta vazia:

   ```cmd
   dism /Get-WimInfo /WimFile:"C:\WinPE_amd64\media\sources\boot.wim"
   mkdir "C:\WinPE_amd64\mount"
   dism /Mount-Image /ImageFile:"C:\WinPE_amd64\media\sources\boot.wim" /Index:1 /MountDir:"C:\WinPE_amd64\mount"
   mkdir "C:\WinPE_amd64\mount\AzulPE"
   copy "C:\Downloads\AzulPE\AzulPE.exe" "C:\WinPE_amd64\mount\AzulPE\AzulPE.exe"
   ```

2. **Escolha uma única forma de inicialização.**
   - **Recomendada para testar:** preserve o `Windows\System32\startnet.cmd` existente (inclusive a chamada ao `wpeinit`) e adicione a linha abaixo ao final, antes de qualquer `exit`. Ao fechar o Azul PE, você volta ao prompt. Em uma imagem WinPE já personalizada, confirme antes se o `startnet.cmd` é executado. Exemplo: `integracao/startnet.exemplo.cmd`.

     ```cmd
     if exist "%SystemDrive%\AzulPE\AzulPE.exe" start "" /wait "%SystemDrive%\AzulPE\AzulPE.exe"
     ```

   - **Como shell principal:** o modelo `integracao/winpeshl.exemplo.ini` chama `wpeinit` e, depois, o Azul PE. Copie-o como `Windows\System32\winpeshl.ini` **dentro da imagem montada**, depois de verificar se não há um shell personalizado que precise ser preservado. Ao escolher esta opção, não adicione também a chamada ao Azul PE no `startnet.cmd`. Encerrar o último aplicativo de `winpeshl.ini` pode encerrar ou reiniciar a sessão WinPE; para manter acesso ao prompt prefira `startnet.cmd`.

3. Feche as janelas que usam o diretório de montagem e salve as mudanças:

   ```cmd
   dism /Unmount-Image /MountDir:"C:\WinPE_amd64\mount" /Commit
   ```

   Para cancelar a montagem **sem salvar**, use `/Discard` no lugar de `/Commit`. Use a mídia já criada, ou gere uma ISO com `MakeWinPEMedia /ISO C:\WinPE_amd64 C:\WinPE-Azul.iso` no ambiente de ferramentas do ADK. O pacote não inclui WinPE, ADK ou uma ISO.

## Integração automatizada no WinPE (Linux)

Em vez de montar a imagem manualmente com DISM no Windows, use o `gerar-imagem-azul.sh`, que gera uma nova ISO com o **Azul PE como shell padrão** (`winpeshl.ini`). Esse fluxo é próprio e **separado** do processo descrito em `../fluxodebuild.md`: o script não usa `settemplate` nem embute os templates do `wpetool` (`menu_iniciar.exe`, `explorer-x64.exe`).

```bash
# 1. Gera a ISO com o Azul PE como shell padrão (compila o AzulPE.exe se preciso)
./gerar-imagem-azul.sh ../WinPE_amd64.iso ../WinPE_azul.iso --compilar

# 2. Teste na VM
../run-winpe-amd64.sh ../WinPE_azul.iso
```

Opções: `--compilar` força a compilação do `AzulPE.exe` antes de injetar; `--testar` abre a ISO gerada na VM ao final.

O que o script faz:
1. Compila `AzulPE.exe`, quando não existe ou quando `--compilar` é passado
2. Monta o `boot.wim` descompactado em `wpe-azul-shell/` (via `wpe-tool.sh workdir`)
3. Copia `AzulPE.exe` para `\AzulPE\` e instala `integracao/winpeshl.ini` como `\Windows\System32\winpeshl.ini` — o WinPE executa `wpeinit` e depois o Azul PE, que passa a ser o shell da sessão. Um `winpeshl.ini` já existente na imagem de entrada (inclusive o dos templates do wpetool) é substituído
4. Recomprime o WIM em LZX e gera a nova ISO in-place (preserva a bootabilidade)

Com `winpeshl.ini` presente, o `startnet.cmd` da imagem **não** é executado, e ao sair do Azul PE a sessão WinPE encerra. Se preferir sair do Azul PE e voltar ao prompt de comando, use a variante `./build-winpe.sh`, que mantém o Azul PE sendo lançado pelo `startnet.cmd`:

```bash
# Variante startnet: Azul PE lançado pelo startnet.cmd (volta ao prompt ao sair)
./build-winpe.sh ../WinPE_amd64.iso ../WinPE_azul_prompt.iso
```

Referências: [montar e personalizar o WinPE](https://learn.microsoft.com/pt-br/windows-hardware/manufacture/desktop/winpe-mount-and-customize?view=windows-11) · [sintaxe de winpeshl.ini](https://learn.microsoft.com/pt-br/windows-hardware/manufacture/desktop/winpeshlini-reference-launching-an-app-when-winpe-starts?view=windows-11).

## Código-fonte e verificação

- `src/`: janelas Win32, pintura GDI, navegação e operações de arquivo.
- `assets/`: recursos incorporados, incluindo as fontes Inter (licença em `assets/LICENCA-Inter.txt`). `tools/gerar_recursos.py` regenera o papel de parede e os ícones; não é necessário executá-lo para compilar.
- **Linux/Debian:** instale `g++-mingw-w64-x86-64-posix`, `binutils-mingw-w64-x86-64` e `make`. Na pasta do projeto, execute `make PREFIX=x86_64-w64-mingw32-` (se houver múltiplas variantes do compilador: `make PREFIX=x86_64-w64-mingw32- CXX=x86_64-w64-mingw32-g++-posix`).
- **Windows/MSYS2:** no terminal **MINGW64** (não UCRT64), instale `make` e `mingw-w64-x86_64-gcc`; na pasta do projeto execute `make PREFIX=`.
- **Teste de fumaça:** `bash tests/fumaca-wine.sh` com Wine64, Xvfb, xauth, Openbox e xdotool. Use `AZULPE_TEST_TMP=/var/tmp` se `/tmp` tiver pouco espaço para um prefixo Wine descartável.
- **Integridade:** `SHA256.txt` contém o hash de `AzulPE.exe`. No Windows comum, confira com `Get-FileHash .\AzulPE.exe -Algorithm SHA256`.

O binário fornecido é um executável **PE32+ x86-64**, com runtime C++ ligado estaticamente e importações apenas de bibliotecas do Windows. A interface e as operações principais foram testadas no Wine, com dados descartáveis; **o boot real do WinPE e as ações de energia ainda precisam ser validados na sua imagem/VM**.
