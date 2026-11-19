#!/usr/bin/env bash
# Teste de fumaça com arquivos descartáveis. Requer Wine64, Xvfb, Openbox e xdotool.
set -euo pipefail
export LANG=C.UTF-8 LC_ALL=C.UTF-8
project=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
if [[ ${1:-} != --x11 ]]; then
    exec xvfb-run -a -s '-screen 0 1440x900x24 -dpi 96' bash "$project/tests/fumaca-wine.sh" --x11
fi
WINE=${WINE:-$(command -v wine64 || command -v wine || printf '%s' /usr/lib/wine/wine64)}
WINESERVER=${WINESERVER:-$(command -v wineserver || printf '%s' /usr/lib/wine/wineserver)}
tmp=$(mktemp -d "${AZULPE_TEST_TMP:-${TMPDIR:-/tmp}}/azulpe-teste.XXXXXXXX")
unset TMPDIR
export WINEPREFIX="$tmp/wine" WINEARCH=win64 WINEDEBUG=-all
wm=0
cleanup() {
    "$WINESERVER" -k 2>/dev/null || true
    ((wm == 0)) || kill "$wm" 2>/dev/null || true
    rm -rf -- "$tmp"
}
trap cleanup EXIT
mkdir "$WINEPREFIX"
openbox --sm-disable >"$tmp/openbox.log" 2>&1 & wm=$!
"$WINE" wineboot -u >"$tmp/boot.log" 2>&1 || { cat "$tmp/boot.log" >&2; exit 1; }
cd -- "$WINEPREFIX/drive_c"
mkdir -p AzulPE TesteAzul/{Dados,Destino}
cp "$project/AzulPE.exe" AzulPE/
printf 'Sessão de teste. UTF-8: ã, é, ç.\n' >TesteAzul/Sessão.txt
printf 'Conteúdo em subpasta\n' >TesteAzul/Dados/Catalogo.txt
"$WINE" 'C:\AzulPE\AzulPE.exe' >"$tmp/app.log" 2>&1 &

need() {
    local pattern=$1
    for ((i=0;i<45;i++)); do
        if xdotool search --onlyvisible --name "$pattern" >/dev/null 2>&1; then sleep .15; return 0; fi
        sleep .13
    done
    printf 'Janela não encontrada: %s\n' "$pattern" >&2
    xdotool search --onlyvisible --name '.' getwindowname %@ >&2 || true
    return 1
}
absent() { ! xdotool search --onlyvisible --name "$1" >/dev/null 2>&1; }
key() { xdotool key --clearmodifiers "$@"; sleep .13; }
type() { xdotool type --clearmodifiers --delay 18 -- "$1"; sleep .10; }
go() { key ctrl+l; type "$1"; key Return; }
select_name() { key ctrl+f; type "$1"; key Return; }
wait_file() { for ((i=0;i<25;i++)); do [[ -e "$1" ]] && { sleep .20; return 0; }; sleep .12; done; echo "Não criado: $1" >&2; return 1; }
start() { xdotool mousemove 55 873 click 1; need '^Iniciar — Azul PE$'; }

need '^Área de Trabalho — Azul PE$'
absent 'Explorador Azul$'
xdotool mousemove 62 113 click --repeat 2 1
need '^Este PC — Explorador Azul$'
start; key Escape; absent '^Iniciar — Azul PE$'
start; type 'pasta do shell'; key Return
need '^AzulPE — Explorador Azul$'
absent '^Iniciar — Azul PE$'
echo 'OK: desktop, duplo clique, menu Iniciar, pesquisa de programas'

go 'C:\TesteAzul'; need '^TesteAzul — Explorador Azul$'
key alt+Left; need '^AzulPE — Explorador Azul$'
key alt+Right; need '^TesteAzul — Explorador Azul$'
select_name 'sessao'; key ctrl+c
go 'C:\TesteAzul\Destino'; need '^Destino — Explorador Azul$'; key ctrl+v
wait_file TesteAzul/Destino/Sessão.txt
cmp TesteAzul/Sessão.txt TesteAzul/Destino/Sessão.txt
key ctrl+v; need '^Azul PE$'; key Return
after=$(cat TesteAzul/Destino/Sessão.txt)
[[ "$after" == "$(cat TesteAzul/Sessão.txt)" ]]
echo 'OK: endereço, histórico, filtro Unicode, cópia de arquivo, sem sobrescrita'

go 'C:\TesteAzul'; select_name 'Dados'; key ctrl+c
go 'C:\TesteAzul\Dados'; key ctrl+v; need '^Azul PE$'; key Return
[[ ! -d TesteAzul/Dados/Dados ]]
go '..\Destino'; need '^Destino — Explorador Azul$'; key ctrl+v
wait_file TesteAzul/Destino/Dados/Catalogo.txt
cmp TesteAzul/Dados/Catalogo.txt TesteAzul/Destino/Dados/Catalogo.txt
echo 'OK: cópia de pasta, bloqueio de autocópia e endereço relativo'

key ctrl+shift+n; need '^Nova pasta$'; type 'Pasta de teste'; key Return
wait_file 'TesteAzul/Destino/Pasta de teste'
key F2; need '^Renomear$'; type 'Renomeada'; key Return
wait_file 'TesteAzul/Destino/Renomeada'; [[ ! -e 'TesteAzul/Destino/Pasta de teste' ]]
key Delete; need '^Confirmar exclusão$'; key Return
[[ -d 'TesteAzul/Destino/Renomeada' ]]
key Delete; need '^Confirmar exclusão$'; key alt+y
[[ ! -e 'TesteAzul/Destino/Renomeada' ]]
select_name 'Dados'; key Delete; need '^Confirmar exclusão$'; key alt+y
need '^Azul PE$'; key Return; [[ -f TesteAzul/Destino/Dados/Catalogo.txt ]]
echo 'OK: criar, renomear, cancelar/confirmar exclusão e preservar pasta não vazia'

select_name 'sessao'; key Return
need '^Sessão.txt — Leitor de texto$'; key alt+F4; absent '^Sessão.txt — Leitor de texto$'
xdotool mousemove 1430 874 click 1; sleep .32; absent 'Explorador Azul$'
xdotool mousemove 150 874 click 1; need '^Destino — Explorador Azul$'
echo 'OK: leitor de texto e botões da barra de tarefas'

start; xdotool mousemove 360 823 click 1; need '^Encerrar Azul PE$'; key alt+y
absent '^Área de Trabalho — Azul PE$'
echo 'OK: fechar o shell sem desligar o computador'
