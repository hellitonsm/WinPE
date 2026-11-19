#!/usr/bin/env bash
# gerar-imagem-azul.sh - Gera uma nova ISO do WinPE com o Azul PE como shell padrao.
#
# Fluxo proprio, separado do processo descrito em ../fluxodebuild.md: nao usa
# wpetool/templates nem 'settemplate' (menu_iniciar / explorer-x64). A imagem
# gerada inicia com wpeinit e depois o Azul PE, atraves de winpeshl.ini.
#
# Uso: ./gerar-imagem-azul.sh <ISO_entrada> [ISO_saida] [--compilar] [--testar]
#
# Exemplo:
#   ./gerar-imagem-azul.sh ../WinPE_amd64.iso ../WinPE_azul.iso --compilar
#
# Requisitos:
#   - wpetool/wpe-tool.sh e dependencias (7z, python3; wimlib ja vem no wpetool)
#   - AzulPE.exe compilado (make PREFIX=x86_64-w64-mingw32-) ou use --compilar
set -euo pipefail

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$DIR/.." && pwd)"
WPETOOL="$PROJECT_ROOT/wpetool/wpe-tool.sh"
WORK="$PROJECT_ROOT/wpe-azul-shell"
PREFIX="${PREFIX-x86_64-w64-mingw32-}"

usage() {
    cat <<EOF
Uso: $0 <ISO_entrada> [ISO_saida] [--compilar] [--testar]

Gera uma nova ISO do WinPE com o Azul PE como shell padrao (winpeshl.ini),
em fluxo separado do ../fluxodebuild.md.

Exemplo:
  $0 ../WinPE_amd64.iso ../WinPE_azul.iso --compilar

Opcoes:
  --compilar   Compila o AzulPE.exe (make) antes de injetar
  --testar     Abre a ISO gerada na VM (run-winpe-amd64.sh) ao final
EOF
}

abspath() {
    case "$1" in
        /*) printf '%s\n' "$1" ;;
        *)  printf '%s\n' "$PWD/$1" ;;
    esac
}

COMPILAR=0
TESTAR=0
ARGS=()
for arg in "$@"; do
    case "$arg" in
        --compilar) COMPILAR=1 ;;
        --testar)   TESTAR=1 ;;
        -h|--help)  usage; exit 0 ;;
        -*)         echo "Opcao desconhecida: $arg" >&2; usage >&2; exit 1 ;;
        *)          ARGS+=("$arg") ;;
    esac
done

ISO_IN="${ARGS[0]:-}"
if [[ -z "$ISO_IN" ]]; then
    usage >&2
    exit 1
fi
if [[ ! -f "$ISO_IN" ]]; then
    echo "Erro: ISO de entrada nao encontrada: $ISO_IN" >&2
    exit 1
fi

ISO_OUT_DISPLAY="${ARGS[1]:-${ISO_IN%.iso}_azul.iso}"
if [[ "$ISO_OUT_DISPLAY" == "$ISO_IN" ]]; then
    echo "Erro: ISO de saida deve ser diferente da de entrada" >&2
    exit 1
fi

ISO_IN="$(abspath "$ISO_IN")"
ISO_OUT="$(abspath "$ISO_OUT_DISPLAY")"
if [[ ! -d "$(dirname "$ISO_OUT")" ]]; then
    echo "Erro: pasta de saida nao existe: $(dirname "$ISO_OUT")" >&2
    exit 1
fi

if [[ ! -x "$WPETOOL" ]]; then
    echo "Erro: wpetool nao encontrado: $WPETOOL" >&2
    exit 1
fi

STATE="$PROJECT_ROOT/.wpetool-work/$(basename "$WORK")/state"
if [[ -f "$STATE" ]]; then
    echo "Erro: $WORK ja esta montada de uma execucao anterior." >&2
    echo "  Descarte com: $WPETOOL unmount $WORK discard" >&2
    exit 1
fi
if [[ -e "$WORK" && -n "$(ls -A "$WORK")" ]]; then
    echo "Erro: pasta de trabalho nao esta vazia: $WORK" >&2
    echo "  Remova-a (ou desmonte) antes de rodar o script de novo." >&2
    exit 1
fi
# Sobrou vazia de uma execucao anterior (o build desmonta e deixa a pasta):
# remove para poder montar de novo.
if [[ -e "$WORK" ]]; then
    rmdir "$WORK"
fi

on_erro() {
    local status=$?
    echo "" >&2
    echo "Falha no processo (codigo $status)." >&2
    if [[ -e "$WORK" ]]; then
        echo "Se a pasta de trabalho ficou montada, descarte com:" >&2
        echo "  $WPETOOL unmount $WORK discard" >&2
    fi
    exit "$status"
}
trap on_erro ERR

echo "=== Gerando imagem do Azul PE (shell padrao) ==="
echo "ISO de entrada    : $ISO_IN"
echo "ISO de saida      : $ISO_OUT_DISPLAY"
echo "Pasta de trabalho: $WORK"
echo ""

# 1. Garante o executavel pronto para injetar
echo "[1/4] Verificando AzulPE.exe..."
if [[ "$COMPILAR" -eq 1 || ! -f "$DIR/AzulPE.exe" ]]; then
    make -C "$DIR" PREFIX="$PREFIX"
fi
if [[ ! -f "$DIR/AzulPE.exe" ]]; then
    echo "Erro: AzulPE.exe nao encontrado apos a compilacao." >&2
    exit 1
fi

# 2. Monta o boot.wim em uma pasta de trabalho editavel
echo "[2/4] Montando boot.wim..."
"$WPETOOL" workdir "$ISO_IN" "$WORK"

# 3. Instala o executavel e define o Azul PE como shell padrao da imagem
echo "[3/4] Instalando AzulPE como shell padrao..."
mkdir -p "$WORK/AzulPE"
cp "$DIR/AzulPE.exe" "$WORK/AzulPE/AzulPE.exe"
cp "$DIR/integracao/winpeshl.ini" "$WORK/Windows/System32/winpeshl.ini"
echo "  - AzulPE.exe  -> \\AzulPE\\AzulPE.exe"
echo "  - winpeshl.ini -> \\Windows\\System32\\winpeshl.ini (wpeinit + AzulPE.exe)"

# 4. Gera a nova ISO
echo "[4/4] Gerando nova ISO..."
"$WPETOOL" build "$WORK" "$ISO_OUT"

# Limpa sobras vazias da montagem (o wpe-tool remove o estado, mas deixa a pasta)
rmdir "$WORK" 2>/dev/null || true
rmdir "$PROJECT_ROOT/.wpetool-work" 2>/dev/null || true

if [[ "$TESTAR" -eq 1 ]]; then
    echo ""
    echo "Abrindo a ISO na VM..."
    "$PROJECT_ROOT/run-winpe-amd64.sh" "$ISO_OUT"
fi

echo ""
echo "Concluido! ISO gerada: $ISO_OUT"
echo "Shell padrao da imagem: Azul PE (winpeshl.ini -> wpeinit + AzulPE.exe)"
echo "Para testar: $PROJECT_ROOT/run-winpe-amd64.sh $ISO_OUT"
