#!/usr/bin/env python3
"""Substitui bytes em uma ISO preservando o restante do arquivo.

Uso: iso_patch_bytes.py ISO_ORIGEM ARQUIVO_NOVO OFFSET ISO_SAIDA [PAD_SIZE]
Se PAD_SIZE for informado, o arquivo novo e preenchido com zeros ate esse
tamanho (util para substituir boot.wim por uma versao menor).
"""
import os
import sys

src, newfile, offset, dst = sys.argv[1:5]
offset = int(offset)
pad_to = int(sys.argv[5]) if len(sys.argv) > 5 else None

with open(newfile, 'rb') as f:
    new = bytearray(f.read())
if pad_to is not None:
    if len(new) > pad_to:
        sys.exit(f'ERRO: arquivo novo ({len(new)}) e maior que o espaco original ({pad_to})')
    new.extend(b'\x00' * (pad_to - len(new)))

with open(src, 'rb') as f:
    data = bytearray(f.read())
if offset + len(new) > len(data):
    sys.exit('ERRO: offset fora dos limites da ISO')
data[offset:offset + len(new)] = new
with open(dst, 'wb') as f:
    f.write(data)
print(f'ISO gravada: {dst} ({os.path.getsize(dst)} bytes)')
