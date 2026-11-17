#!/usr/bin/env bash
# Lançador: WinPE 10 AMD64 no VirtualBox
# Usa a ISO corrigida (com winpeshl.ini explicito, evita o loop de reboot da ISO original).
set -euo pipefail

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
VM="WinPE10-AMD64"
ISO_ARG="${1:-$DIR/WinPE_amd64_v10.0.19041_shellfix.iso}"
if [[ "$ISO_ARG" = /* ]]; then
    ISO="$ISO_ARG"
else
    ISO="$DIR/$ISO_ARG"
fi

if ! command -v VBoxManage >/dev/null 2>&1; then
    echo "Erro: VirtualBox (VBoxManage) não encontrado." >&2
    exit 1
fi

if [[ ! -f "$ISO" ]]; then
    echo "Erro: ISO não encontrada: $ISO" >&2
    exit 1
fi

# Cria a VM se ainda não existir
if ! VBoxManage showvminfo "$VM" >/dev/null 2>&1; then
    echo "Criando VM '$VM'..."
    VBoxManage createvm --name "$VM" --ostype Windows10_64 --register
fi

if VBoxManage showvminfo "$VM" --machinereadable | grep -q '^VMState="running"'; then
    CURRENT="$(VBoxManage showvminfo "$VM" | grep -oE 'Location:[[:space:]]*"[^"]+\.iso"' | head -n 1 | sed 's/.*"\(.*\)"/\1/')"
    if [[ "$CURRENT" == "$ISO" ]]; then
        echo "VM já está em execução com essa ISO."
        exit 0
    fi
    echo "VM em execução com outra ISO — desligando para trocar..."
    VBoxManage controlvm "$VM" poweroff
    sleep 2
fi

VBoxManage modifyvm "$VM" \
    --memory 4096 \
    --cpus 2 \
    --vram 32 \
    --graphicscontroller vmsvga \
    --firmware bios \
    --boot1 dvd \
    --boot2 disk \
    --boot3 none \
    --boot4 none \
    --paravirt-provider default \
    --audio-driver pulse --audiocontroller hda \
    --clipboard-mode bidirectional \
    --mouse ps2 --keyboard ps2 \
    --usb-ohci off \
    --uart1 off

# Anexa a ISO corrigida (controlador IDE, slot secundário)
VBoxManage storagectl "$VM" --name "IDE" --add ide --bootable on 2>/dev/null || true
if VBoxManage showvminfo "$VM" | grep -q 'Port 1, Unit 0'; then
    VBoxManage storageattach "$VM" --storagectl "IDE" --port 1 --device 0 --medium none
fi
VBoxManage storageattach "$VM" --storagectl "IDE" --port 1 --device 0 \
    --type dvddrive --medium "$ISO"

echo "Iniciando '$VM' com o WinPE AMD64 (ISO corrigida)..."
exec VBoxManage startvm "$VM" --type gui
