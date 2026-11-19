#!/usr/bin/env bash
# build-winpe.sh - Injeta o Azul PE em uma imagem WinPE (boot.wim) e gera uma ISO.
#
# Variante "startnet": o Azul PE e lancado pelo startnet.cmd e, ao sair dele,
# voce volta ao prompt de comando do WinPE. Para deixar o Azul PE como shell
# padrao da imagem, use ./gerar-imagem-azul.sh (fluxo separado).
#
# Uso: ./build-winpe.sh <ISO_entrada> [ISO_saida]
#
# Exemplo:
#   ./build-winpe.sh WinPE_amd64.iso WinPE_azul.iso
#
# Requisitos:
#   - wpetool/wpe-tool.sh e dependências (7z, wimlib, python3)
#   - AzulPE.exe compilado (make PREFIX=x86_64-w64-mingw32-)
set -euo pipefail

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$DIR/.." && pwd)"
WPETOOL="$PROJECT_ROOT/wpetool/wpe-tool.sh"

ISO_IN="${1:-}"
if [[ -z "$ISO_IN" ]]; then
    echo "Uso: $0 <ISO_entrada> [ISO_saida]"
    echo "Exemplo: $0 WinPE_amd64.iso WinPE_azul.iso"
    exit 1
fi

if [[ ! -f "$ISO_IN" ]]; then
    echo "Erro: ISO de entrada não encontrada: $ISO_IN" >&2
    exit 1
fi

ISO_OUT="${2:-${ISO_IN%.iso}_azul.iso}"
WORK="wpe-azul"

STATE="$(dirname "$WORK")/.wpetool-work/$(basename "$WORK")/state"
if [[ -f "$STATE" ]]; then
    echo "Erro: $WORK ja esta montada (use 'unmount' ou remova-a)" >&2
    exit 1
fi
if [[ -e "$WORK" && -n "$(ls -A "$WORK")" ]]; then
    echo "Erro: pasta de trabalho nao esta vazia: $WORK" >&2
    exit 1
fi
# Sobrou vazia de uma execucao anterior: remove para poder montar de novo
if [[ -e "$WORK" ]]; then
    rmdir "$WORK"
fi

AZULPE_EXE="$DIR/AzulPE.exe"
if [[ ! -f "$AZULPE_EXE" ]]; then
    echo "Erro: AzulPE.exe nao encontrado. Compile primeiro com:" >&2
    echo "  make PREFIX=x86_64-w64-mingw32-" >&2
    exit 1
fi

echo "=== Injetando Azul PE na imagem WinPE ==="
echo "ISO de entrada : $ISO_IN"
echo "ISO de saida   : $ISO_OUT"
echo "Pasta de trabalho: $WORK"
echo ""

# 1. Monta o boot.wim em uma pasta de trabalho editável
echo "[1/4] Montando boot.wim..."
"$WPETOOL" workdir "$ISO_IN" "$WORK"

# 2. Copia o AzulPE.exe para \AzulPE\ dentro da imagem
echo "[2/4] Copiando AzulPE.exe para \\AzulPE\\..."
mkdir -p "$WORK/AzulPE"
cp "$AZULPE_EXE" "$WORK/AzulPE/AzulPE.exe"

# 3. Instala o startnet.cmd que inicializa o WinPE e lança o Azul PE
echo "[3/4] Instalando startnet.cmd..."
cp "$DIR/integracao/startnet.cmd" "$WORK/Windows/System32/startnet.cmd"

# 4. Gera a nova ISO
echo "[4/4] Gerando ISO..."
"$WPETOOL" build "$WORK" "$ISO_OUT"

# Limpa sobras vazias da montagem (o wpe-tool remove o estado, mas deixa a pasta)
rmdir "$WORK" 2>/dev/null || true
rmdir "$(dirname "$WORK")/.wpetool-work" 2>/dev/null || true

echo ""
echo "Concluido! ISO gerada: $ISO_OUT"
echo "Para testar: ./run-winpe-amd64.sh $ISO_OUT"
