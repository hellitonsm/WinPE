#!/usr/bin/env python3
"""Edita elementos do BCD (registry hive) dentro de um arquivo BCD.

Uso: hivedit.py BCD_IN BCD_OUT OBJECT ELEMENT=HEX [ELEMENT=HEX ...]

OBJECT pode ser:
  bootloader | {7619dcc9-fafe-11d9-b411-000476eba25f}  (entrada do WinPE)
  hypervisor | {7ff607e0-4395-11db-b0de-0800200c9a66}  (config do hipervisor)
"""
import hivex
import sys

OBJECTS = {
    'bootloader': '{7619dcc9-fafe-11d9-b411-000476eba25f}',
    'hypervisor': '{7ff607e0-4395-11db-b0de-0800200c9a66}',
}

src, dst, obj = sys.argv[1:4]
obj = OBJECTS.get(obj.lower(), obj)
changes = []
for arg in sys.argv[4:]:
    if '=' not in arg:
        sys.exit(f'argumento invalido: {arg} (use ELEMENT=HEX)')
    element, hexval = arg.split('=', 1)
    changes.append((element, bytes.fromhex(hexval)))

h = hivex.Hivex(src, write=True)
try:
    objs = h.node_get_child(h.root(), 'Objects')
    node = h.node_get_child(objs, obj)
    if node is None:
        sys.exit(f'objeto nao encontrado no BCD: {obj}')
    els = h.node_get_child(node, 'Elements')
    if els is None:
        sys.exit('objeto sem Elements')
    for element, data in changes:
        e = h.node_get_child(els, element)
        if e is None:
            e = h.node_add_child(els, element)
        h.node_set_value(e, {'key': 'Element', 't': 3, 'value': data})
        print(f'  {obj} / {element} = {data.hex()}')
    h.commit(dst)
finally:
    h.close()
print('BCD salvo:', dst)
