#!/usr/bin/env python3
"""Substitui todas as copias de um arquivo dentro de uma ISO preservando offsets.

Uso: iso_replace_all.py ISO_ORIGEM ARQUIVO_NOVO ISO_SAIDA
O arquivo novo deve ter o mesmo tamanho do original (ex.: BCD, 256 KiB).
"""
import sys

src, newfile, dst = sys.argv[1:4]
with open(newfile, 'rb') as f:
    new = f.read()
with open(src, 'rb') as f:
    data = bytearray(f.read())

pattern = new[:4096]
offs = []
pos = 0
while True:
    pos = data.find(pattern, pos)
    if pos < 0:
        break
    if bytes(data[pos:pos + len(new)]) == new:
        offs.append(pos)
    pos += 1

if not offs:
    sys.exit('ERRO: arquivo original nao encontrado na ISO')
for off in offs:
    data[off:off + len(new)] = new
    print(f'  substituido @ {off}')
with open(dst, 'wb') as f:
    f.write(data)
print('ISO gravada:', dst)
