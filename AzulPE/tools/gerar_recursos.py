#!/usr/bin/env python3
"""Cria a arte original de Azul PE. Dependências: numpy, pillow, cairosvg.
Os binários de recursos gerados já estão incluídos no projeto.
"""
import io
from pathlib import Path

import cairosvg
import numpy as np
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
ASSETS = ROOT / 'assets'
ASSETS.mkdir(exist_ok=True)


def wallpaper():
    w, h = 1680, 1050
    Y, X = np.mgrid[0:h, 0:w].astype(np.float32)
    X /= w - 1
    Y /= h - 1
    rgb = np.empty((h, w, 3), dtype=np.float32)
    for i, (a, b) in enumerate(zip((5, 23, 60), (7, 50, 106))):
        rgb[:, :, i] = a + (b - a) * Y ** 1.27

    def add(color, alpha):
        for k, v in enumerate(color):
            rgb[:, :, k] += v * alpha

    def fog(x, y, wx, wy):
        return np.exp(-(((X-x)/wx)**2+((Y-y)/wy)**2)/2)

    def dip(distance, spread):
        return np.exp(-(distance / spread)**2 / 2)

    add((5, 45, 89), fog(.95, .60, .45, .35))
    add((0, 8, 20), fog(.44, .13, .52, .36))
    add((7, 34, 68), fog(.70, .92, .36, .115))
    add((-1, -9, -17), fog(.09, .15, .44, .32))
    add((-1, -10, -16), fog(.23, .93, .36, .13))
    # Auroras de vidro. Espessura calculada na normal de cada curva.
    ridge = .894 - .056 * X - .700 * X ** 4.85
    n = (Y-ridge) / np.sqrt(1+(.056+3.395*X**3.85)**2*.625**2)
    strength = (.25+.62*X+.53*np.exp(-((X-.75)/.24)**2))
    add((8, 100, 143), dip(n-.025, .092)*strength*.53)
    add((18, 53, 75), dip(n-.054, .048)*strength*.69)
    add((33, 75, 82), dip(n-.026, .029)*strength*.54)
    add((111, 128, 105), dip(n-.002, .015)*strength*.59)
    add((45, 58, 54), dip(n, .00155)*strength*.60)
    add((36, 50, 53), dip(n-.044, .006)*strength*.25)
    add((0, -15, -18), dip(n+.023, .012)*X*.48)
    # A lâmina inferior cruza o brilho principal.
    sweep = .932 - .083 * X - .129 * X ** 2.32
    q = (Y-sweep) / (1+.06*X)
    add((15, 74, 101), dip(q-.008, .06)*(.32+.50*X))
    add((40, 60, 65), dip(q, .006)*(.2+.52*X))
    add((57, 104, 88), dip(q, .00145)*(.16+.63*X))
    bottom = .881+.045*X+.023*X**2
    z = Y-bottom
    add((4, 38, 80), dip(z-.009, .017)*(.23+.68*X))
    add((37, 61, 58), dip(z, .0018)*(.11+.22*X))
    add((39, 57, 36), fog(.80, .742, .13, .068)*.56)
    add((-1, -18, -27), np.clip((Y-.96)*17, 0, 1))
    # Ombros escuros, luz azul no horizonte e dithering sutil em 8 bits.
    add((-3, -16, -25), fog(-.15, .55, .38, .64)*.47)
    noise = np.random.default_rng(613).uniform(-.58, .58, (h, w))
    arr = np.uint8(np.clip(rgb + noise[:, :, None], 0, 255))
    im = Image.fromarray(arr, 'RGB')
    b = io.BytesIO()
    im.save(b, format='BMP')
    (ASSETS / 'parede.dib').write_bytes(b.getvalue()[14:])


BASE = '''
<linearGradient id="gold" x2=".18" y2="1" gradientUnits="objectBoundingBox"><stop stop-color="#fff1a3"/><stop offset=".45" stop-color="#ffd05c"/><stop offset="1" stop-color="#d88a24"/></linearGradient>
<linearGradient id="goldT" x2="0" y2="1" gradientUnits="objectBoundingBox"><stop stop-color="#ffde72"/><stop offset="1" stop-color="#ba791c"/></linearGradient>
<linearGradient id="ice" x2=".11" y2="1" gradientUnits="objectBoundingBox"><stop stop-color="#e0f8ff"/><stop offset=".44" stop-color="#72cbfb"/><stop offset="1" stop-color="#2679c3"/></linearGradient>
<linearGradient id="blue" x2=".2" y2="1" gradientUnits="objectBoundingBox"><stop stop-color="#66bbef"/><stop offset="1" stop-color="#175e9f"/></linearGradient>
<linearGradient id="glass" x2=".22" y2="1" gradientUnits="objectBoundingBox"><stop stop-color="#275784"/><stop offset=".46" stop-color="#102d50"/><stop offset="1" stop-color="#08192e"/></linearGradient>
<linearGradient id="steel" x2=".1" y2="1" gradientUnits="objectBoundingBox"><stop stop-color="#f9feff"/><stop offset=".48" stop-color="#ccdce9"/><stop offset="1" stop-color="#7a94ae"/></linearGradient>
<linearGradient id="dark" x2=".15" y2="1" gradientUnits="objectBoundingBox"><stop stop-color="#63809c"/><stop offset=".52" stop-color="#2e4765"/><stop offset="1" stop-color="#142840"/></linearGradient>
<linearGradient id="purple" x2=".2" y2="1" gradientUnits="objectBoundingBox"><stop stop-color="#f0a9ff"/><stop offset=".51" stop-color="#a35cdc"/><stop offset="1" stop-color="#613591"/></linearGradient>
<linearGradient id="paper" x2=".14" y2="1" gradientUnits="objectBoundingBox"><stop stop-color="#fff"/><stop offset="1" stop-color="#c6ddf4"/></linearGradient>
'''

def icon(content):
    # Percentages keep the gradients in the bounding box in SVG renderers.
    fixed = BASE.replace('y2="1"','y2="100%"')
    for old, new in [('.18','18%'), ('.11','11%'), ('.2','20%'), ('.22','22%'), ('.1','10%'), ('.15','15%'), ('.14','14%')]:
        fixed = fixed.replace(f'x2="{old}"', f'x2="{new}"')
    return f'<svg xmlns="http://www.w3.org/2000/svg" width="64" height="64" viewBox="0 0 64 64"><defs>{fixed}</defs>{content}</svg>'

FOLDER = '''<path d="M5 16q0-4 4-4h17l6 6h21q4 0 4 4v26q0 4-4 4H9q-4 0-4-4Z" fill="url(#goldT)" stroke="#89521f" stroke-width="1.4"/>
<path d="M6 26q0-4 4-4h46q4 0 3 4l-3 23q-.5 3-4 3H10q-4 0-4-4Z" fill="url(#gold)" stroke="#956027" stroke-width="1.15"/>
<path d="M9 27q0-2 3-2h43" stroke="#fff1af" stroke-width="2" stroke-linecap="round" fill="none"/>
<path d="M10 47h42" stroke="#ffce63" stroke-width="1.4" opacity=".53"/>'''

# Índices sincronizados com Icon em src/base.hpp.
ART = {
'brand': '''<rect x="5" y="5" width="54" height="54" rx="12" fill="url(#glass)" stroke="#6fb7e2" stroke-width="1.3"/>
<path d="M6 17Q6 6 17 6h30q11 0 11 11v11Q36 36 6 30Z" fill="#aedaff" opacity=".10"/>
<path d="M15 19l14-3v16H15zm17-4l17-4v21H32zm-17 20h14v15l-14-3zm17 0h17v21l-17-5Z" fill="url(#ice)"/><path d="M15 19l14-3v2l-14 3zm17-4l17-4v2l-17 4z" fill="#d8f5ff"/>''',
'folder': FOLDER,
'docs': FOLDER.replace('goldT', 'blue').replace('gold','ice').replace('#89521f','#1a4b79').replace('#956027','#1c5689').replace('#fff1af','#daf6ff').replace('#ffce63','#aee5fd') + '''<path d="M24 30h12l5 5v12H24Z" fill="url(#glass)" opacity=".95"/><path d="M36 30v6h5Z" fill="#6fa9d6"/><path d="M28 39h9m-9 4h9" stroke="#c8ecff" opacity=".55" stroke-width="1"/>''',
'pc': '''<path d="M26 42h12v11l9 3v3H17v-3l9-3Z" fill="url(#steel)" stroke="#405878" stroke-width="1.5"/>
<rect x="5" y="9" width="54" height="37" rx="3.5" fill="url(#steel)" stroke="#203650" stroke-width="1.6"/>
<rect x="8.5" y="12.5" width="47" height="28" rx="1" fill="url(#glass)"/>
<path d="M9 37Q25 41 55 16v12Q34 42 9 39Z" fill="url(#blue)" opacity=".9"/>
<path d="M10 39q29 1 45-18" fill="none" stroke="#79d2fd" stroke-width="1.3" opacity=".8"/>
<circle cx="51" cy="43" r="1.15" fill="#1d9ecc"/>''',
'network': '''<circle cx="32" cy="32" r="26" fill="url(#purple)" stroke="#5d2a85" stroke-width="1.5"/>
<circle cx="32" cy="32" r="22.5" fill="none" stroke="#f3d3ff" stroke-opacity=".43" stroke-width="1.4"/>
<circle cx="32" cy="32" r="16" fill="none" stroke="#fff" stroke-width="2.1"/>
<ellipse cx="32" cy="32" rx="7.4" ry="16" fill="none" stroke="#fff" stroke-width="1.7"/>
<path d="M17 27h30m-30 10h30" stroke="#fff" stroke-width="1.65"/>
<circle cx="39" cy="37" r="3.4" fill="#fff"/>''',
'console': '''<rect x="5" y="11" width="54" height="43" rx="4" fill="url(#steel)" stroke="#2b4665" stroke-width="1.5"/>
<rect x="8" y="21" width="48" height="29" rx="1.5" fill="url(#glass)"/>
<path d="M15 28l9 6-9 6m14 0h10" fill="none" stroke="#e0f2ff" stroke-width="2.5" stroke-linecap="round" stroke-linejoin="round"/>
<path d="M11 16h10m25 0h2m4 0h2" stroke="#406885" stroke-width="1.8" stroke-linecap="round"/>''',
'note': '''<path d="M11 10q0-3 3-3h30l9 9v36q0 4-4 4H15q-4 0-4-4Z" fill="url(#paper)" stroke="#426182" stroke-width="1.5"/>
<path d="M44 7v10h9" fill="#8eafcc" stroke="#5f86a8"/>
<path d="M18 14v37m4-27h24m-24 7h20m-20 7h22m-22 7h17" fill="none" stroke="#adc1d4" stroke-width="1.5"/>
<path d="M31 47l3-8 19-22q2-2 5 0l2 2q2 2 0 4L41 45Z" fill="url(#gold)" stroke="#645030" stroke-width="1.35"/><path d="M34 40l6 5-9 2Z" fill="#253a53"/>
<path d="M51 20l6 5" stroke="#fff1c8" stroke-width="2"/>''',
'drive': '''<path d="M11 17q1-3 4-3h34q3 0 4 3l7 22v9q0 4-4 4H8q-4 0-4-4v-9Z" fill="url(#steel)" stroke="#385574" stroke-width="1.5"/>
<path d="M7 37h50l2 4v6q0 4-4 4H9q-4 0-4-4v-6Z" fill="url(#dark)" stroke="#294662" stroke-width="1"/>
<path d="M11 42h23m-23 4h17" stroke="#8ba5bb" stroke-width="1.45" stroke-linecap="round"/>
<path d="M10 34h44" stroke="#f1fbff" stroke-width="1.3"/>
<circle cx="50" cy="44" r="2.25" fill="#88e97d" stroke="#114d3c" stroke-width=".85"/>''',
'usb': '''<rect x="24" y="4" width="16" height="17" rx="1.5" fill="url(#steel)" stroke="#385776" stroke-width="1.25"/>
<path d="M28 8v4m8-4v4" stroke="#486584" stroke-width="2.3"/>
<rect x="20" y="18" width="24" height="40" rx="6" fill="url(#blue)" stroke="#1b4975" stroke-width="1.4"/>
<path d="M25 23h14" stroke="#a3def6" stroke-width="1.5" stroke-linecap="round"/>
<path d="M32 46V28m0 13l-5-5v-2m5 4l5-3v-2m-7-3l2-2 2 2" stroke="#dcf4ff" stroke-width="1.5" fill="none" stroke-linejoin="round" stroke-linecap="round"/>
<circle cx="32" cy="47" r="2" fill="#d2f4ff"/>''',
'file': '''<path d="M14 6h25l13 13v35q0 4-4 4H15q-4 0-4-4V10q0-4 3-4Z" fill="url(#paper)" stroke="#5d7b9c" stroke-width="1.65"/>
<path d="M39 7v13h12" fill="#9fbcd8" stroke="#7293b5" stroke-width="1.1"/>
<path d="M18 28h26m-26 6h26m-26 6h26m-26 6h16" stroke="#849fbc" stroke-width="1.5" stroke-linecap="round"/>''',
'info': '''<circle cx="32" cy="32" r="25" fill="url(#blue)" stroke="#284e83" stroke-width="1.45"/><circle cx="32" cy="32" r="22" fill="none" stroke="#9fddff" stroke-opacity=".65" stroke-width="1.6"/>
<circle cx="32" cy="22" r="3.25" fill="#fff"/><path d="M32 30v14m-4-14h4m-4 14h8" stroke="#fff" stroke-width="3.8" stroke-linecap="round"/>''',
'desktop': '''<rect x="5" y="11" width="54" height="42" rx="4" fill="url(#steel)" stroke="#284464" stroke-width="1.5"/>
<rect x="8.5" y="14.5" width="47" height="35" rx="1.5" fill="url(#glass)"/>
<path d="M9 43Q34 46 55 20v21Q33 46 9 46Z" fill="url(#blue)"/>
<path d="M10 43q25 3 45-17" fill="none" stroke="#8be7ff" stroke-width="1.6" opacity=".85"/>
<path d="M9 46h46v3H9Z" fill="#123153"/><path d="M12 47h7" stroke="#8fdcf5" stroke-width="1.7" stroke-linecap="round"/>''',
'apps': '''<rect x="7" y="7" width="50" height="50" rx="5" fill="url(#paper)" stroke="#58799a" stroke-width="1.5"/>
<rect x="13" y="13" width="17" height="17" rx="2" fill="url(#blue)"/><rect x="34" y="13" width="17" height="17" rx="2" fill="url(#gold)"/><rect x="13" y="34" width="17" height="17" rx="2" fill="url(#purple)"/><rect x="34" y="34" width="17" height="17" rx="2" fill="#56ae8c"/>''',
'power': '''<circle cx="32" cy="32" r="26" fill="url(#dark)" stroke="#89adca" stroke-width="1.4"/>
<path d="M32 16v17m-9-12a15 15 0 1 0 18 0" fill="none" stroke="#edf7ff" stroke-width="3.4" stroke-linecap="round"/>''',
}


def assets():
    out = bytearray()
    for key, body in ART.items():
        im = Image.open(io.BytesIO(cairosvg.svg2png(bytestring=icon(body).encode(), output_width=192, output_height=192)))
        im = im.convert('RGBA').resize((64,64), Image.Resampling.LANCZOS)
        if key == 'brand':
            im.save(ASSETS / 'azul.ico', sizes=[(16,16),(24,24),(32,32),(48,48),(64,64)])
        arr = np.asarray(im, dtype=np.uint32)
        bgra = arr[:, :, (2,1,0,3)].copy()
        bgra[:, :, :3] = (bgra[:, :, :3]*bgra[:, :, 3:]+127)//255
        out += bgra.astype(np.uint8).tobytes()
    (ASSETS / 'icones.bgra').write_bytes(out)


if __name__ == '__main__':
    wallpaper()
    assets()
