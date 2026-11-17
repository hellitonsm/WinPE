#!/usr/bin/env bash
# wpe-tool - ferramenta para editar imagens Windows PE (boot.wim / ISO)
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
LIB="$HERE/lib"
TPL="$HERE/templates"
export LD_LIBRARY_PATH="$LIB/lib:${LD_LIBRARY_PATH:-}"
export PYTHONPATH="$LIB/python:${PYTHONPATH:-}"
WIMLIB="$LIB/bin/wimlib-imagex"

BOOTSHELL_OBJ='{7619dcc9-fafe-11d9-b411-000476eba25f}'
HYPERVISOR_OBJ='{7ff607e0-4395-11db-b0de-0800200c9a66}'

die() { echo "erro: $*" >&2; exit 1; }
say() { echo "$*"; }
tmpdir() { mktemp -d "${TMPDIR:-/tmp}/wpetool.XXXXXXXX"; }
WORKDIR=""
cleanup() { if [[ -n "$WORKDIR" ]]; then rm -rf "$WORKDIR"; fi; return 0; }
trap cleanup EXIT

check_deps() {
    for c in 7z isoinfo python3; do
        command -v "$c" >/dev/null 2>&1 || die "'$c' não encontrado (instale: p7zip-full, genisoimage)"
    done
    [[ -x "$WIMLIB" ]] || die "wimlib-imagex não encontrado em $WIMLIB"
    [[ -f "$LIB/lib/libwim.so.15" ]] || die "biblioteca libwim ausente"
}

wim_update() {
    local wim="$1"; shift
    printf '%s\n' "$@" | "$WIMLIB" update "$wim" 1 >/dev/null
}

extract_wim() {
    local iso="$1" work="$2"
    say "Extraindo sources/boot.wim da ISO..."
    7z e -y -o"$work" "$iso" sources/boot.wim >/dev/null
    [[ -f "$work/boot.wim" ]] || die "boot.wim não encontrado na ISO"
}

inject_wim() {
    local iso="$1" wim="$2" out="$3"
    local info off size
    info="$(python3 "$LIB/iso_wim.py" "$iso")"
    read -r off size <<<"$info"
    say "Substituindo boot.wim @ $off (tamanho original: $size bytes)"
    python3 "$LIB/iso_patch_bytes.py" "$iso" "$wim" "$off" "$out" "$size"
}

workroot_for() {
    local mnt="${1%/}"
    echo "$(cd "$(dirname "$mnt")" && pwd)/.wpetool-work/$(basename "$mnt")"
}

read_state() {
    local mnt="$1" root key value
    root="$(workroot_for "$mnt")"
    [[ -f "$root/state" ]] || die "montagem não encontrada em $mnt (rode 'workdir' primeiro)"
    ST_ROOT="$root"
    while IFS='=' read -r key value; do
        case "$key" in
            ISO) ST_ISO="$value" ;;
            WIM) ST_WIM="$value" ;;
            OFFSET) ST_OFFSET="$value" ;;
            SIZE) ST_SIZE="$value" ;;
        esac
    done < "$root/state"
}

cmd_workdir() {
    check_deps
    local iso="$1" mnt="${2%/}"
    [[ -f "$iso" ]] || die "ISO não encontrada: $iso"
    [[ -n "$mnt" ]] || die "informe a pasta de trabalho"
    local root; root="$(workroot_for "$mnt")"
    [[ ! -f "$root/state" ]] || die "$mnt já está preparada (use 'build' ou 'unmount')"
    mkdir -p "$mnt" "$root"
    extract_wim "$iso" "$root"
    say "Montando boot.wim (leitura/escrita) em $mnt ..."
    "$WIMLIB" mountrw "$root/boot.wim" 1 "$mnt"
    local off size info
    info="$(python3 "$LIB/iso_wim.py" "$iso")" || die "não foi possível localizar boot.wim na ISO"
    read -r off size <<<"$info"
    [[ -n "$off" && -n "$size" ]] || die "falha ao detectar boot.wim na ISO"
    cat > "$root/state" <<EOF
ISO=$iso
WIM=$root/boot.wim
OFFSET=$off
SIZE=$size
EOF
    say "Pronto! Edite os arquivos em: $mnt"
    say "Depois rode: $HERE/wpe-tool.sh build $mnt nova.iso"
}

cmd_unmount() {
    check_deps
    local mnt="${1%/}" mode="${2:-commit}"
    read_state "$mnt"
    if [[ "$mode" == "discard" ]]; then
        say "Descartando alterações..."
        "$WIMLIB" unmount "$mnt"
    else
        say "Salvando alterações no WIM..."
        "$WIMLIB" unmount --commit "$mnt"
    fi
    rm -rf "$ST_ROOT"
    say "Pasta $mnt desmontada."
}

cmd_build() {
    check_deps
    local mnt="${1%/}" out="$2"
    [[ -n "$out" ]] || die "informe o arquivo de ISO de saída"
    read_state "$mnt"
    say "Salvando alterações no WIM..."
    "$WIMLIB" unmount --commit "$mnt"
    say "Recomprimindo WIM (pode demorar alguns minutos)..."
    "$WIMLIB" optimize "$ST_WIM" --recompress --compress=lzx >/dev/null
    say "Gerando nova ISO..."
    python3 "$LIB/iso_patch_bytes.py" "$ST_ISO" "$ST_WIM" "$ST_OFFSET" "$out" "$ST_SIZE"
    rm -rf "$ST_ROOT"
    say "Concluído: $out"
}

patch_bcd_iso() {
    local iso="$1" out="$2"; shift 2
    local work bcd
    work="$(tmpdir)"; WORKDIR="$work"
    7z e -y -o"$work" "$iso" Boot/BCD >/dev/null
    bcd="$work/BCD"
    [[ -f "$bcd" ]] || die "Boot/BCD não encontrado na ISO"
    say "Aplicando alterações no BCD..."
    python3 "$LIB/hivedit.py" "$bcd" "$work/BCD.new" "$@"
    python3 "$LIB/iso_replace_all.py" "$iso" "$work/BCD.new" "$out"
}

cmd_help() {
    cat <<'EOF'
wpe-tool - edição de imagens Windows PE (boot.wim / ISO)

Uso: wpe-tool.sh <comando> [argumentos]

Comandos:
  info ISO                      Mostra informações da ISO e do boot.wim
  extract ISO [PASTA]           Extrai ISO e boot.wim para uma pasta de trabalho
  workdir ISO PASTA             Monta boot.wim descompactado em PASTA (editável)
  build PASTA SAIDA             Salva a pasta editada e gera uma nova ISO
  unmount PASTA [discard]       Desmonta a pasta (commit ou descarta alterações)
  list WIM [INDICE]             Lista arquivos dentro do WIM (padrão: 1)
  wim-add WIM ARQUIVO CAMINHO   Adiciona/substitui arquivo no WIM
  wim-delete WIM CAMINHO        Remove arquivo do WIM (use --recursive para pastas)
  wim-optimize WIM              Recomprime o WIM em LZX (reduz tamanho)
  inject ISO WIM SAIDA          Substitui boot.wim dentro da ISO (preserva boot)
   fix-shell ISO [SAIDA]         CORREÇÃO do loop de reboot: adiciona winpeshl.ini
                                 que mantém o cmd.exe aberto
   settemplate [PASTA]           Copia todos os arquivos de wpetool/templates/ para
                                 PASTA (padrão: wpe-work/Windows/System32/)
   bcd-patch ISO SAIDA OBJ ELEMENTO=HEX...
                                Edita elementos do BCD (OBJ = bootloader | hypervisor)
  bcd-nocrashautoreboot ISO SAIDA   Mostra BSOD em vez de reiniciar
  bcd-hv-off ISO SAIDA              Desativa hypervisorlaunchtype
  bcd-serial-debug ISO SAIDA        Ativa debug do kernel na COM1 (115200)

Exemplos:
  ./wpe-tool.sh info WinPE_amd64.iso
  ./wpe-tool.sh fix-shell WinPE_amd64.iso WinPE_corrigida.iso
  ./wpe-tool.sh extract WinPE_amd64.iso meu_trabalho
  ./wpe-tool.sh wim-add meu_trabalho/boot.wim ./meu_script.cmd '\\Windows\\System32\\startnet.cmd'
  ./wpe-tool.sh inject WinPE_amd64.iso meu_trabalho/boot.wim nova.iso
  ./wpe-tool.sh settemplate wpe-work/Windows/System32
  ./wpe-tool.sh build wpe-work WinPE_menu.iso

Caminhos dentro do WIM usam barras normais, ex.: /Windows/System32/startnet.cmd
EOF
}

cmd_info() {
    check_deps
    local iso="$1"
    [[ -f "$iso" ]] || die "ISO não encontrada: $iso"
    say "=== ISO ==="
    isoinfo -d -i "$iso" 2>/dev/null | grep -iE 'volume id|volume set id|publisher|application|eltorito|bootid|bootoff|logical block|volume size' || true
    say ""
    say "=== boot.wim ==="
    read -r off size <<<"$(python3 "$LIB/iso_wim.py" "$iso")"
    say "offset: $off | tamanho: $size bytes ($(awk -v s="$size" 'BEGIN{printf "%.1f", s/1048576}') MiB)"
    local work; work="$(tmpdir)"
    extract_wim "$iso" "$work"
    "$WIMLIB" info "$work/boot.wim" 1 2>/dev/null | head -n 14 || true
    rm -rf "$work"
}

cmd_extract() {
    check_deps
    local iso="$1" work="${2:-${1%.iso}.work}"
    [[ -f "$iso" ]] || die "ISO não encontrada: $iso"
    mkdir -p "$work"
    say "Extraindo conteúdo da ISO para $work ..."
    7z x -y -o"$work" "$iso" >/dev/null
    extract_wim "$iso" "$work"
    say "Pronto. boot.wim em: $work/boot.wim"
    say "Dica: use 'wim-add', 'list' e depois 'inject' para gerar uma nova ISO."
}

cmd_list() {
    check_deps
    local wim="$1" idx="${2:-1}"
    [[ -f "$wim" ]] || die "WIM não encontrado: $wim"
    "$WIMLIB" dir "$wim" "$idx"
}

cmd_wim_add() {
    check_deps
    local wim="$1" src="$2" dest="$3"
    [[ -f "$wim" ]] || die "WIM não encontrado: $wim"
    [[ -f "$src" ]] || die "Arquivo de origem não encontrado: $src"
    local dest_win="${dest//\//\\}"
    if "$WIMLIB" dir "$wim" 1 2>/dev/null | grep -Fqx "$dest"; then
        wim_update "$wim" "delete $dest"
    fi
    say "Adicionando $src -> $dest"
    wim_update "$wim" "add $src $dest_win"
}

cmd_wim_delete() {
    check_deps
    local wim="$1" dest="$2" flag="${3:-}"
    [[ -f "$wim" ]] || die "WIM não encontrado: $wim"
    local cmd="delete $dest"
    [[ "$flag" == "--recursive" ]] && cmd="delete --recursive $dest"
    wim_update "$wim" "$cmd"
    say "Removido: $dest"
}

cmd_wim_optimize() {
    check_deps
    local wim="$1"
    [[ -f "$wim" ]] || die "WIM não encontrado: $wim"
    say "Recomprimindo WIM (LZX, pode demorar alguns minutos)..."
    "$WIMLIB" optimize "$wim" --recompress --compress=lzx >/dev/null
    say "Tamanho final: $(stat -c%s "$wim") bytes"
}

cmd_inject() {
    check_deps
    local iso="$1" wim="$2" out="$3"
    [[ -f "$iso" ]] || die "ISO não encontrada: $iso"
    [[ -f "$wim" ]] || die "WIM não encontrado: $wim"
    [[ "$iso" != "$out" ]] || die "arquivo de saída deve ser diferente da ISO de entrada"
    inject_wim "$iso" "$wim" "$out"
}

cmd_fix_shell() {
    check_deps
    local iso="$1" out="${2:-${1%.iso}_shellfix.iso}"
    [[ -f "$iso" ]] || die "ISO não encontrada: $iso"
    [[ "$iso" != "$out" ]] || die "arquivo de saída deve ser diferente da ISO de entrada"
    local work; work="$(tmpdir)"; WORKDIR="$work"
    extract_wim "$iso" "$work"
    local wim="$work/boot.wim"

    say "Aplicando correção do shell..."
    wim_update "$wim" \
        "add $TPL/winpeshl.ini /Windows/System32/winpeshl.ini" \
        "delete /Windows/System32/startnet.cmd" \
        "add $TPL/startnet.cmd /Windows/System32/startnet.cmd"
    say "Recomprimindo WIM para caber no espaço original..."
    "$WIMLIB" optimize "$wim" --recompress --compress=lzx >/dev/null
    inject_wim "$iso" "$wim" "$out"
    say "Pronto! Use: VBoxManage storageattach <vm> --storagectl IDE --port 1 --device 0 --medium '$out'"
}

cmd_settemplate() {
    check_deps
    local dest="${1:-wpe-work/Windows/System32}"
    [[ -d "$TPL" ]] || die "pasta de templates não encontrada: $TPL"
    [[ -d "$dest" ]] || mkdir -p "$dest" || die "não foi possível criar $dest"
    local n=0
    for f in "$TPL"/*; do
        [[ -f "$f" ]] || continue
        cp "$f" "$dest/"
        n=$((n + 1))
        say "Embutido $(basename "$f") em $dest"
    done
    [[ "$n" -gt 0 ]] || die "nenhum arquivo encontrado em $TPL"
    say "Concluído: $n arquivo(s) embutido(s) em $dest"
    say "Dica: depois rode './wpe-tool.sh build wpe-work nova.iso' e './run-winpe-amd64.sh nova.iso'"
}

main() {
    [[ $# -ge 1 ]] || { cmd_help; exit 1; }
    local cmd="$1"; shift
    case "$cmd" in
        help|-h|--help) cmd_help ;;
        info) cmd_info "$@" ;;
        extract) cmd_extract "$@" ;;
        workdir|mount) cmd_workdir "$@" ;;
        build) cmd_build "$@" ;;
        unmount) cmd_unmount "$@" ;;
        list) cmd_list "$@" ;;
        wim-add) cmd_wim_add "$@" ;;
        wim-delete) cmd_wim_delete "$@" ;;
        wim-optimize) cmd_wim_optimize "$@" ;;
        inject) cmd_inject "$@" ;;
        fix-shell) cmd_fix_shell "$@" ;;
        settemplate) cmd_settemplate "$@" ;;
        bcd-patch) patch_bcd_iso "$@" ;;
        bcd-nocrashautoreboot)
            patch_bcd_iso "$1" "$2" "$BOOTSHELL_OBJ" '26000024=01' ;;
        bcd-hv-off)
            patch_bcd_iso "$1" "$2" "$HYPERVISOR_OBJ" '250000f4=0000000000000000' ;;
        bcd-serial-debug)
            patch_bcd_iso "$1" "$2" "$BOOTSHELL_OBJ" \
                '15000011=0000000000000000' \
                '15000013=0100000000000000' \
                '15000014=00c2010000000000' ;;
        *) die "comando desconhecido: $cmd (use 'help')" ;;
    esac
}

main "$@"
