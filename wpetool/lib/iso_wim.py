#!/usr/bin/env python3
"""Localiza a extensão (offset, tamanho) de sources/boot.wim dentro de uma ISO 9660.

Tenta primeiro a entrada do diretório ISO9660; se não encontrar (ISOs com
layouts diferentes), usa a assinatura MSWIM alinhada a setor + o tamanho
informado pelo 7z.
"""
import re
import subprocess
import sys

path = sys.argv[1]
with open(path, 'rb') as f:
    data = f.read()

# 1) entrada de diretório ISO9660
found = []
for m in re.finditer(b'BOOT.WIM', data, re.IGNORECASE):
    p = m.start()
    rec = p - 33
    if rec < 0 or data[rec] < 33 or data[rec] > 255 or data[rec + 1] != 0:
        continue
    lba = int.from_bytes(data[rec + 2:rec + 6], 'little')
    size = int.from_bytes(data[rec + 10:rec + 14], 'little')
    off = lba * 2048
    if (off + 8 <= len(data) and data[off:off + 8] == b'MSWIM\x00\x00\x00'
            and 1024 * 1024 < size < 4 * 1024 ** 3):
        found.append((off, size))

# 2) fallback: assinatura MSWIM + tamanho via 7z
if not found:
    out = subprocess.run(['7z', 'l', '-ba', path, 'sources/boot.wim'],
                         capture_output=True, text=True).stdout.strip()
    if not out:
        sys.exit('ERRO: sources/boot.wim nao encontrado na ISO')
    size = int(out.split()[3])
    offsets = [i for i in range(0, len(data) - 8, 2048)
               if data[i:i + 8] == b'MSWIM\x00\x00\x00']
    if not offsets:
        sys.exit('ERRO: cabecalho MSWIM nao encontrado na ISO')
    found = [(offsets[0], size)]

off, size = found[0]
print(off, size)
