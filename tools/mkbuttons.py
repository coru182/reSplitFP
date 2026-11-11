#!/usr/bin/env python3
"""reSplitFP: the PlayStation buttons drawn in place of TimeSplitters: Future Perfect's Xbox ones (original art,
GPL-3.0, from reSplit2's tools/mkbuttons.py).

Writes game/art/*.png, four times the game's own sizes (the controller twice), and game/src/game/tsfp_button_art.cpp
with them built in:
  cross / circle / square / triangle.png  128x128, for the game's 32x32 A / B / X / Y buttons
  l1 / r1.png                             128x128, for its 32x32 White and Black buttons
  l2 / r2.png                             128x128, for its 32x32 left and right trigger signs
  sheet.png                               512x256, for its 128x64 sheet: B A X Y, Start and Back, two arrows
  pad.png                                 1024x512, for its 512x256 controller picture (the Controls menu)
  pad_glow.png                            256x256, for the 128x128 glow drawn behind that picture
Needs rsvg-convert (librsvg2-bin).   python3 tools/mkbuttons.py

Your own controller picture instead of the drawing, for your own builds (it stays out of the repository):
  python3 tools/mkbuttons.py --image my-controller.png
      a controller, front view, on a white or transparent background: cut out and fitted where the game draws its
      controller, into art/pad.png and art/pad_glow.png (the top-level art/, which git ignores). Needs Pillow and
      numpy. build-ps5.sh then builds them in over game/art's (python3 tools/mkbuttons.py --embed FILE); delete
      art/pad.png to go back to the drawing.
"""
import argparse
import os
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ART = os.path.join(ROOT, 'game', 'art')
LOCAL = os.path.join(ROOT, 'art')  # your own pictures (--image), never in the repository
# the pictures built into the program: their names there and their files
BUILT_IN = (('Cross', 'cross'), ('Circle', 'circle'), ('Square', 'square'), ('Triangle', 'triangle'), ('L1', 'l1'),
            ('R1', 'r1'), ('L2', 'l2'), ('R2', 'r2'), ('Sheet', 'sheet'), ('Pad', 'pad'), ('Glow', 'pad_glow'))

# the PlayStation colours: Cross blue, Circle red, Square pink, Triangle green
COLOURS = {'cross': '#8fb3ff', 'circle': '#ff6d6d', 'square': '#ff9ade', 'triangle': '#3ee0a8'}


def button(kind, cx, cy, r):
    """a dark glossy face button of radius r at (cx, cy), its symbol in its colour"""
    c = COLOURS[kind]
    s = r * 0.46  # the symbol's half size
    w = r * 0.155  # its stroke
    if kind == 'cross':
        sym = (f'<path d="M{cx-s},{cy-s} L{cx+s},{cy+s} M{cx+s},{cy-s} L{cx-s},{cy+s}" stroke="{c}" '
               f'stroke-width="{w}" stroke-linecap="round" fill="none"/>')
    elif kind == 'circle':
        sym = f'<circle cx="{cx}" cy="{cy}" r="{s*0.95}" stroke="{c}" stroke-width="{w}" fill="none"/>'
    elif kind == 'square':
        q = s * 0.82
        sym = (f'<rect x="{cx-q}" y="{cy-q}" width="{2*q}" height="{2*q}" rx="{w*0.25}" stroke="{c}" '
               f'stroke-width="{w}" fill="none"/>')
    else:
        t = s * 1.05
        sym = (f'<path d="M{cx},{cy-t} L{cx+t*0.98},{cy+t*0.68} L{cx-t*0.98},{cy+t*0.68} Z" stroke="{c}" '
               f'stroke-width="{w}" stroke-linejoin="round" fill="none"/>')
    gid = f'g{kind}{int(cx)}{int(cy)}'
    return f'''
  <defs>
    <radialGradient id="{gid}" cx="40%" cy="32%" r="75%">
      <stop offset="0" stop-color="#5a606c"/><stop offset="0.55" stop-color="#262a31"/><stop offset="1" stop-color="#0d0f12"/>
    </radialGradient>
    <linearGradient id="{gid}h" x1="0" y1="0" x2="0" y2="1">
      <stop offset="0" stop-color="#ffffff" stop-opacity="0.55"/><stop offset="1" stop-color="#ffffff" stop-opacity="0"/>
    </linearGradient>
  </defs>
  <circle cx="{cx}" cy="{cy+r*0.06}" r="{r}" fill="#000" opacity="0.45"/>
  <circle cx="{cx}" cy="{cy}" r="{r}" fill="url(#{gid})" stroke="#07080a" stroke-width="{r*0.06}"/>
  <circle cx="{cx}" cy="{cy}" r="{r*0.9}" fill="none" stroke="#7d8594" stroke-opacity="0.35" stroke-width="{r*0.04}"/>
  <g filter="url(#glow)">{sym}</g>
  <ellipse cx="{cx-r*0.12}" cy="{cy-r*0.48}" rx="{r*0.55}" ry="{r*0.3}" fill="url(#{gid}h)"/>'''


GLOW = '''<defs><filter id="glow" x="-50%" y="-50%" width="200%" height="200%"><feGaussianBlur stdDeviation="2.2" result="b"/>
  <feMerge><feMergeNode in="b"/><feMergeNode in="SourceGraphic"/></feMerge></filter></defs>'''


def pill(x0, y0, x1, y1, label, lx, ly, ink='#ffffff'):
    """a small dark pill button (Options, the touch pad) with its name under it"""
    h = y1 - y0
    return f'''
  <rect x="{x0}" y="{y0+3}" width="{x1-x0}" height="{h}" rx="{h/2}" fill="#000" opacity="0.4"/>
  <rect x="{x0}" y="{y0}" width="{x1-x0}" height="{h}" rx="{h/2}" fill="#23272e" stroke="#07080a" stroke-width="3"/>
  <rect x="{x0+8}" y="{y0+6}" width="{x1-x0-16}" height="{h*0.35}" rx="{h*0.17}" fill="#ffffff" opacity="0.18"/>
  <text x="{lx}" y="{ly}" text-anchor="middle" font-family="DejaVu Sans" font-weight="bold" font-size="24"
        fill="{ink}" letter-spacing="0.5">{label}</text>'''


def shoulder(name):
    """L1 / R1 (128x128): a dark bumper with its name, where the game's round White and Black buttons were"""
    return f'''
  <defs><linearGradient id="sh" x1="0" y1="0" x2="0" y2="1">
    <stop offset="0" stop-color="#5a606c"/><stop offset="0.6" stop-color="#262a31"/><stop offset="1" stop-color="#121418"/>
  </linearGradient></defs>
  <rect x="10" y="33" width="108" height="68" rx="30" fill="#000" opacity="0.45"/>
  <rect x="10" y="28" width="108" height="68" rx="30" fill="url(#sh)" stroke="#07080a" stroke-width="5"/>
  <rect x="26" y="34" width="76" height="14" rx="7" fill="#fff" opacity="0.16"/>
  <text x="64" y="80" text-anchor="middle" font-family="DejaVu Sans" font-weight="bold" font-size="44"
        fill="#ffffff">{name}</text>'''


def trigger(name):
    """L2 / R2 (128x128): a dark trigger with its name, where the game's L and R trigger signs were"""
    return f'''
  <defs><linearGradient id="tr" x1="0" y1="0" x2="0" y2="1">
    <stop offset="0" stop-color="#5a606c"/><stop offset="0.55" stop-color="#262a31"/><stop offset="1" stop-color="#121418"/>
  </linearGradient></defs>
  <path d="M22,122 L22,52 C22,22 40,10 64,10 C88,10 106,22 106,52 L106,122 Z" transform="translate(0,3)" fill="#000"
        opacity="0.45"/>
  <path d="M22,120 L22,52 C22,22 40,10 64,10 C88,10 106,22 106,52 L106,120 Z" fill="url(#tr)" stroke="#07080a"
        stroke-width="5" stroke-linejoin="round"/>
  <path d="M38,30 C46,20 82,20 90,30" fill="none" stroke="#fff" stroke-opacity="0.2" stroke-width="7"
        stroke-linecap="round"/>
  <text x="64" y="90" text-anchor="middle" font-family="DejaVu Sans" font-weight="bold" font-size="44"
        fill="#ffffff">{name}</text>'''


def svg(w, h, body, scale=1):
    return (f'<svg xmlns="http://www.w3.org/2000/svg" width="{w*scale}" height="{h*scale}" viewBox="0 0 {w} {h}">'
            f'{GLOW}{body}</svg>')


def render(name, text):
    src = os.path.join(ART, name + '.svg')
    with open(src, 'w') as f:
        f.write(text)
    out = os.path.join(ART, name + '.png')
    subprocess.run(['rsvg-convert', src, '-o', out], check=True)
    os.remove(src)
    return out


# The controller picture: reSplitFP's own controller, white with PlayStation buttons on it. The game draws its
# 512x256 picture 170 x 144 of its 640x480 frame, which the PS5 shows 16:9: a texel is 1.2706 times taller than it is
# wide as seen. The controller is drawn in its own proportions in a 403 x 256 space (as wide as the Xbox one there,
# 354, centred on 201.5, 127) and widened to the texture's 512. The drawing below is in a 512 x 512 space around
# (256, 273), 496 x 336 across.
STRETCH = 512 / 403
FIT = f'scale({STRETCH:.6f},1) translate(201.5,127) scale(0.696) translate(-256,-273)'


def mirrored(right):
    """a closed outline symmetric about x 256, from its right half: a start, then cubic segments (c1, c2, end)"""
    start, segs = right[0], right[1:]
    d = f'M{start[0]},{start[1]} ' + ''.join(f'C{c1[0]},{c1[1]} {c2[0]},{c2[1]} {e[0]},{e[1]} ' for c1, c2, e in segs)
    ends = [start] + [e for _, _, e in segs]
    for i in range(len(segs) - 1, -1, -1):  # back along the left half
        c1, c2, _ = segs[i]
        d += f'C{512-c2[0]},{c2[1]} {512-c1[0]},{c1[1]} {512-ends[i][0]},{ends[i][1]} '
    return d + 'Z'


BODY = mirrored([(256, 340),
                 ((290, 340), (318, 340), (338, 346)),
                 ((362, 354), (378, 378), (394, 404)),
                 ((410, 428), (430, 440), (452, 438)),
                 ((486, 434), (504, 404), (504, 360)),
                 ((504, 296), (496, 228), (482, 184)),
                 ((470, 146), (438, 124), (396, 124)),
                 ((350, 124), (300, 134), (256, 134))])
SHOULDERS = (78, 322)  # L1 and R1: 112 x 40 from y 104

# the symbols a shade deeper than the prompts' (COLOURS), to stand out on white
PAD_COLOURS = {'triangle': '#1aa57c', 'circle': '#e5484d', 'cross': '#4a7de6', 'square': '#d45aae'}

PAD_DEFS = '''<defs>
 <linearGradient id="shell" x1="0" y1="0" x2="0" y2="1">
  <stop offset="0" stop-color="#fbfcfd"/><stop offset="0.55" stop-color="#eceff3"/><stop offset="1" stop-color="#cfd4db"/>
 </linearGradient>
 <linearGradient id="edge" x1="0" y1="0" x2="0" y2="1">
  <stop offset="0.6" stop-color="#000" stop-opacity="0"/><stop offset="1" stop-color="#5b6470" stop-opacity="0.35"/>
 </linearGradient>
 <radialGradient id="cap" cx="45%" cy="35%" r="70%">
  <stop offset="0" stop-color="#ffffff"/><stop offset="1" stop-color="#e1e5ea"/>
 </radialGradient>
 <radialGradient id="stick" cx="50%" cy="40%" r="60%">
  <stop offset="0" stop-color="#3d424b"/><stop offset="1" stop-color="#1d2025"/>
 </radialGradient>
 <radialGradient id="stickcap" cx="45%" cy="38%" r="65%">
  <stop offset="0" stop-color="#4a5059"/><stop offset="1" stop-color="#262a30"/>
 </radialGradient>
 <linearGradient id="shoulder" x1="0" y1="0" x2="0" y2="1">
  <stop offset="0" stop-color="#4a4f58"/><stop offset="1" stop-color="#25282e"/>
 </linearGradient>
 <filter id="shade" x="-20%" y="-20%" width="140%" height="140%"><feGaussianBlur stdDeviation="6"/></filter>
</defs>'''


def face(kind, cx, cy, r):
    """a white face button with its symbol in its colour"""
    c = PAD_COLOURS[kind]
    s, w = r * 0.5, r * 0.16
    if kind == 'cross':
        sym = (f'<path d="M{cx-s},{cy-s} L{cx+s},{cy+s} M{cx+s},{cy-s} L{cx-s},{cy+s}" stroke="{c}" '
               f'stroke-width="{w}" stroke-linecap="round"/>')
    elif kind == 'circle':
        sym = f'<circle cx="{cx}" cy="{cy}" r="{s*0.95}" stroke="{c}" stroke-width="{w}" fill="none"/>'
    elif kind == 'square':
        q = s * 0.8
        sym = (f'<rect x="{cx-q}" y="{cy-q}" width="{2*q}" height="{2*q}" rx="{w*0.3}" stroke="{c}" '
               f'stroke-width="{w}" fill="none"/>')
    else:
        t = s * 1.05
        sym = (f'<path d="M{cx},{cy-t} L{cx+t*0.98},{cy+t*0.7} L{cx-t*0.98},{cy+t*0.7} Z" stroke="{c}" '
               f'stroke-width="{w}" stroke-linejoin="round" fill="none"/>')
    return (f'<circle cx="{cx}" cy="{cy+2.5}" r="{r}" fill="#9aa2ad" opacity="0.6"/>'
            f'<circle cx="{cx}" cy="{cy}" r="{r}" fill="url(#cap)" stroke="#8a929c" stroke-width="2"/>{sym}')


def stick(cx, cy):
    return (f'<circle cx="{cx}" cy="{cy}" r="46" fill="#c9ced6" stroke="#9aa1ab" stroke-width="2"/>'
            f'<circle cx="{cx}" cy="{cy+3}" r="36" fill="#14161a" opacity="0.35"/>'
            f'<circle cx="{cx}" cy="{cy}" r="36" fill="url(#stick)" stroke="#1b1e23" stroke-width="2.5"/>'
            f'<circle cx="{cx}" cy="{cy}" r="25" fill="url(#stickcap)" stroke="#202329" stroke-width="2"/>'
            f'<ellipse cx="{cx-5}" cy="{cy-13}" rx="13" ry="6" fill="#fff" opacity="0.12"/>')


def dpad(cx, cy):
    a, b = 14, 38  # an arm's half width, its reach
    p = (f'M{cx-a},{cy-b} L{cx+a},{cy-b} L{cx+a},{cy-a} L{cx+b},{cy-a} L{cx+b},{cy+a} L{cx+a},{cy+a} '
         f'L{cx+a},{cy+b} L{cx-a},{cy+b} L{cx-a},{cy+a} L{cx-b},{cy+a} L{cx-b},{cy-a} L{cx-a},{cy-a} Z')
    arrows = ''
    for dx, dy in ((0, -1), (0, 1), (-1, 0), (1, 0)):
        tx, ty = cx + dx * 26, cy + dy * 26
        pts = (f'{tx},{ty+dy*6} {tx-5.5},{ty-dy*3} {tx+5.5},{ty-dy*3}' if dx == 0 else
               f'{tx+dx*6},{ty} {tx-dx*3},{ty-5.5} {tx-dx*3},{ty+5.5}')
        arrows += f'<polygon points="{pts}" fill="#3a3f47"/>'
    return (f'<path d="{p}" transform="translate(0,3)" fill="#9aa2ad" opacity="0.6"/>'
            f'<path d="{p}" fill="url(#cap)" stroke="#8a929c" stroke-width="2" stroke-linejoin="round"/>'
            f'<circle cx="{cx}" cy="{cy}" r="6" fill="#d3d8df"/>{arrows}')


def controller():
    s = PAD_DEFS
    for x in SHOULDERS:  # behind the body
        s += f'<rect x="{x}" y="104" width="112" height="40" rx="16" fill="url(#shoulder)" stroke="#1b1e23" stroke-width="2.5"/>'
        s += f'<rect x="{x+14}" y="110" width="84" height="6" rx="3" fill="#fff" opacity="0.18"/>'
    s += f'<path d="{BODY}" transform="translate(0,8)" fill="#000" opacity="0.35" filter="url(#shade)"/>'
    s += f'<path d="{BODY}" fill="url(#shell)" stroke="#7e8691" stroke-width="3"/>'
    s += f'<path d="{BODY}" fill="url(#edge)"/>'
    s += ('<path d="M150,150 C200,142 230,141 256,141 C282,141 312,142 362,150" fill="none" stroke="#fff" '
          'stroke-width="5" stroke-linecap="round" opacity="0.9"/>')  # the top's highlight
    # the centre: two small buttons and a round one, ringed in the menus' blue
    for cx in (196, 316):
        s += f'<rect x="{cx-6}" y="186" width="12" height="26" rx="6" fill="#d3d8df" stroke="#8a929c" stroke-width="2"/>'
    s += ('<circle cx="256" cy="230" r="17" fill="url(#cap)" stroke="#8a929c" stroke-width="2"/>'
          '<circle cx="256" cy="230" r="10" fill="none" stroke="#4fa3f7" stroke-width="3"/>')
    s += dpad(128, 216)
    s += stick(190, 300) + stick(322, 300)
    # the face buttons: Triangle at the top, Circle right, Cross at the bottom, Square left
    for kind, dx, dy in (('triangle', 0, -1), ('circle', 1, 0), ('cross', 0, 1), ('square', -1, 0)):
        s += face(kind, 384 + dx * 36, 216 + dy * 36, 20)
    return f'<g transform="{FIT}">{s}</g>'


def outline():
    """the controller's shape in white, for its glow"""
    s = f'<path d="{BODY}" fill="#fff"/>'
    s += ''.join(f'<rect x="{x}" y="104" width="112" height="40" rx="16" fill="#fff"/>' for x in SHOULDERS)
    return f'<g transform="{FIT}">{s}</g>'


def glow():
    """the 128x128 glow is drawn 178 x 152 from (109.6, 168.8), the picture 170 x 144 from (113.6, 172.8): the outline
    placed as the game places the picture over it, widened a little and blurred, in the glow's grey"""
    place = f'translate({4/178*128:.3f},{4/152*128:.3f}) scale({170/178*128/512:.6f},{144/152*128/256:.6f})'
    return (f'<defs><filter id="soft" x="-30%" y="-30%" width="160%" height="160%">'
            f'<feMorphology operator="dilate" radius="2"/><feGaussianBlur stdDeviation="3"/></filter></defs>'
            f'<g filter="url(#soft)"><g transform="{place}"><g style="fill:#858585">{outline().replace("#fff", "#858585")}'
            f'</g></g></g>')


def from_image(path):
    """art/pad.png and art/pad_glow.png from a picture of a controller, fitted as the drawing is"""
    import numpy as np
    from PIL import Image, ImageDraw, ImageFilter
    im = Image.open(path).convert('RGBA')
    a = np.asarray(im).copy()
    if a[..., 3].min() >= 250:  # a white background: flooded from the corners, so white inside the controller stays
        marked = im.convert('RGB')
        w, h = marked.size
        for xy in [(0, 0), (w - 1, 0), (0, h - 1), (w - 1, h - 1)]:
            if min(marked.getpixel(xy)) >= 240:
                ImageDraw.floodfill(marked, xy, (255, 0, 255), thresh=14)
        m = np.asarray(marked)
        background = (m[..., 0] == 255) & (m[..., 1] == 0) & (m[..., 2] == 255)
        alpha = Image.fromarray(np.where(background, 0, 255).astype(np.uint8))
        alpha = alpha.filter(ImageFilter.MinFilter(3)).filter(ImageFilter.GaussianBlur(0.6))  # no white fringe
        a[..., 3] = np.asarray(alpha)
        im = Image.fromarray(a)
    im = im.crop(im.getbbox())
    # as the player sees it (403 x 256, twice over): within 360 x 240 centred on (201.5, 127), then widened to 512
    k = min(720 / im.width, 480 / im.height)
    size = (round(im.width * k), round(im.height * k))
    shown = Image.new('RGBA', (806, 512), (0, 0, 0, 0))
    shown.paste(im.convert('RGBa').resize(size, Image.LANCZOS).convert('RGBA'), (403 - size[0] // 2, 254 - size[1] // 2))
    pad = shown.convert('RGBa').resize((1024, 512), Image.LANCZOS).convert('RGBA')
    # the glow: its outline placed as the game places the picture over the glow, widened and blurred, in its grey
    outline = Image.new('L', (256, 256), 0)
    outline.paste(pad.getchannel('A').resize((round(170 / 178 * 256), round(144 / 152 * 256)), Image.LANCZOS),
                  (round(4 / 178 * 256), round(4 / 152 * 256)))
    outline = outline.filter(ImageFilter.MaxFilter(5)).filter(ImageFilter.GaussianBlur(6))
    glow = Image.new('RGBA', (256, 256), (133, 133, 133, 0))
    glow.putalpha(outline)
    os.makedirs(LOCAL, exist_ok=True)
    pad.save(os.path.join(LOCAL, 'pad.png'), optimize=True)
    glow.save(os.path.join(LOCAL, 'pad_glow.png'), optimize=True)
    print('mkbuttons: art/pad.png and art/pad_glow.png from', path)


def embed(out, local):
    """the pictures as C++ arrays: game/art's, or with local, art/'s own pad.png and pad_glow.png over them"""
    lines = ['// written by tools/mkbuttons.py from game/art/*.png (reSplitFP\'s own drawings, GPL-3.0)\n',
             '#include <cstddef>\n\nnamespace tsfp::art {\n']
    used = []
    for name, file in BUILT_IN:
        path = os.path.join(ART, file + '.png')
        own = os.path.join(LOCAL, file + '.png')
        if local and file.startswith('pad') and os.path.isfile(os.path.join(LOCAL, 'pad.png')):
            path = own
            used.append('art/' + file + '.png')
        data = open(path, 'rb').read()
        lines.append(f'extern const unsigned char k{name}[];\n')
        lines.append(f'extern const size_t k{name}Size = {len(data)};\n')
        lines.append(f'const unsigned char k{name}[] = {{')
        for i in range(0, len(data), 24):
            lines.append('\n    ' + ','.join(str(b) for b in data[i:i + 24]) + ',')
        lines.append('\n};\n')
    lines.append('}  // namespace tsfp::art\n')
    text = ''.join(lines)
    if not os.path.isfile(out) or open(out).read() != text:  # left alone when the same, so not built again
        with open(out, 'w') as o:
            o.write(text)
    return used


def draw():
    os.makedirs(ART, exist_ok=True)
    # the 32x32 buttons: the game's fill x 1..30, y 1..30 of 32; four times larger
    for kind in COLOURS:
        render(kind, svg(128, 128, button(kind, 62, 62, 56)))
    for name in ('L1', 'R1'):
        render(name.lower(), svg(128, 128, shoulder(name)))
    for name in ('L2', 'R2'):
        render(name.lower(), svg(128, 128, trigger(name)))
    # the 128x64 sheet: B A X Y in its top row (x 1..30, 33..63, 65..94, 97..126; y 1..30), Start and Back buttons
    # (centres 16.5 and 47.5, 43.5) with their names in black under them (y 57..61), and the two white arrows
    body = ''.join(button(k, cx * 4, 62, 56) for k, cx in (('circle', 15.5), ('cross', 48), ('square', 79.5),
                                                              ('triangle', 111.5)))
    body += pill(26, 152, 106, 196, 'OPTIONS', 64, 244, '#000000')
    body += pill(150, 152, 230, 196, 'PAD', 190, 244, '#000000')
    body += '<path d="M284,136 L332,188 L284,240 Z" fill="#e2dee2"/>'  # the right arrow (x 71..83, y 34..60)
    body += '<path d="M428,164 L480,212 L376,212 Z" fill="#e2dde2"/>'  # the up arrow (x 94..120, y 41..53)
    render('sheet', svg(512, 256, body))
    # the Controls menu's controller (512x256, twice: it is drawn large) and its glow (128x128, twice)
    render('pad', svg(512, 256, controller(), 2))
    render('pad_glow', svg(128, 128, glow(), 2))
    # built into the program: always the drawings here, never art/'s own pictures
    out = os.path.join(ROOT, 'game', 'src', 'game', 'tsfp_button_art.cpp')
    embed(out, local=False)
    print('mkbuttons: game/art/*.png and', os.path.relpath(out, ROOT))


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument('--image', help='a picture of a controller, for art/pad.png and art/pad_glow.png')
    p.add_argument('--embed', metavar='FILE', help='only write the C++ arrays to FILE, art/ over game/art')
    args = p.parse_args()
    if args.image:
        from_image(args.image)
    elif args.embed:
        used = embed(args.embed, local=True)
        print('mkbuttons: built in', ', '.join(used) if used else 'game/art/*.png')
    else:
        draw()


if __name__ == '__main__':
    sys.exit(main())
