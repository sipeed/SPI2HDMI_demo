#!/usr/bin/env python3
"""Breadboard wiring of STC8G1K17A (DIP8) + SPI2HDMI + USB-serial adapter on a 170-point mini board -> wiring.svg"""
P = 40                       # hole pitch in px
NC = 17                      # columns of an SYB-170
OX, OY = 470, 170            # hole (col 1, row a)
ROWS = 'abcdefghij'
def X(c): return OX + (c - 1) * P
def Y(r):
    i = ROWS.index(r); return OY + i * P + (2 * P if i >= 5 else 0)
o = []
def wire(x1, y1, x2, y2, col, sag=0, w=7):
    mx, my = (x1 + x2) / 2, (y1 + y2) / 2 + sag
    d = f'M{x1:.1f} {y1:.1f} Q{mx:.1f} {my:.1f} {x2:.1f} {y2:.1f}'
    o.append(f'<path d="{d}" fill="none" stroke="#00000055" stroke-width="{w+4}" stroke-linecap="round"/>')
    o.append(f'<path d="{d}" fill="none" stroke="{col}" stroke-width="{w}" stroke-linecap="round"/>')
    for x, y in ((x1, y1), (x2, y2)): o.append(f'<circle cx="{x:.1f}" cy="{y:.1f}" r="7" fill="#d9d9d9" stroke="#555"/>')
def text(x, y, s, size=15, fill='#222', anchor='middle', weight=400, rot=None):
    t = f' transform="rotate({rot} {x} {y})"' if rot is not None else ''
    o.append(f'<text x="{x:.1f}" y="{y:.1f}" font-size="{size}" fill="{fill}" text-anchor="{anchor}" font-weight="{weight}"{t}>{s}</text>')
W, H = 1400, 860
o.append(f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {W} {H}" width="{W}" height="{H}" font-family="Noto Sans CJK SC, Inter, sans-serif">')
o.append(f'<rect width="{W}" height="{H}" fill="#fff"/>')
text(W / 2, 46, 'STC8G1K17A (DIP8) + SPI2HDMI：170 孔迷你面包板接线', 26, '#111', weight=700)
text(W / 2, 78, '串口模块同时负责供电（5V 给 HDMI 模块，3.3V 给单片机）、下载程序和键盘操控', 16, '#555')
bx0, bx1, by0, by1 = X(1) - P, X(NC) + P, Y('a') - P, Y('j') + P
o.append(f'<rect x="{bx0}" y="{by0}" width="{bx1-bx0}" height="{by1-by0}" rx="16" fill="#f7f6f2" stroke="#cfcabd" stroke-width="2"/>')
o.append(f'<rect x="{X(2)}" y="{Y("e")+P*0.75}" width="{(NC-3)*P}" height="{P*0.5}" rx="{P*0.25}" fill="#e3e0d6"/>')
for r in ROWS:
    for c_ in range(1, NC + 1): o.append(f'<rect x="{X(c_)-6}" y="{Y(r)-6}" width="12" height="12" rx="2.5" fill="#3a3a3a"/>')
    text(X(1) - P * 0.72, Y(r) + 5, r, 13, '#aaa')
for c_ in (1, 5, 10, 15, 17): text(X(c_), Y('a') - P * 0.62, str(c_), 12, '#aaa')
# ---- SPI2HDMI: header pins in rows c and i, columns 13..17, HDMI overhangs to the right ----
mx0, mx1, my0, my1 = X(13) - P * 0.55, X(17) + P * 0.7, Y('c') - P * 0.5, Y('i') + P * 0.5
o.append(f'<path d="M{mx0} {my0} H{mx1-26} L{mx1} {my0+26} V{my1-26} L{mx1-26} {my1} H{mx0} Z" fill="#1c7a4c" stroke="#0e5232" stroke-width="2"/>')
o.append(f'<rect x="{mx1-70}" y="{(my0+my1)/2-108}" width="170" height="216" rx="10" fill="#c8ccd0" stroke="#7b8187" stroke-width="2"/>')
text(mx1 + 16, (my0 + my1) / 2 + 8, 'HDMI', 24, '#444', weight=700)
text(mx0 + 58, (my0 + my1) / 2, 'SPI2HDMI', 20, '#fff', weight=700, rot=-90)
top = ['RST', 'CS', 'SCK', 'GND', '5V']; bot = ['TE', 'IO3', 'IO2', 'IO1', 'IO0']
for i, c_ in enumerate(range(13, 18)):
    for r, lab, dy in (('c', top[i], 30), ('i', bot[i], -20)):
        o.append(f'<circle cx="{X(c_)}" cy="{Y(r)}" r="9" fill="#e8c34a" stroke="#8a6d12"/>')
        text(X(c_), Y(r) + dy, lab, 14, '#eafff3', weight=700)
# ---- DIP8, notch left, columns 6..9, pins in rows e / f ----
cx0, cx1, cy0, cy1 = X(6) - P * 0.5, X(9) + P * 0.5, Y('e') + 12, Y('f') - 12
for c_ in range(6, 10):
    o.append(f'<rect x="{X(c_)-7}" y="{Y("e")-7}" width="14" height="24" fill="#bfc3c7"/><rect x="{X(c_)-7}" y="{Y("f")-17}" width="14" height="24" fill="#bfc3c7"/>')
o.append(f'<rect x="{cx0}" y="{cy0}" width="{cx1-cx0}" height="{cy1-cy0}" rx="5" fill="#26282b"/>')
o.append(f'<path d="M{cx0} {(cy0+cy1)/2-12} A12 12 0 0 1 {cx0} {(cy0+cy1)/2+12}" fill="#55595e"/>')
text((cx0 + cx1) / 2 + 6, (cy0 + cy1) / 2 + 6, 'STC8G1K17A', 17, '#e6e6e6', weight=700)
ptop = ['P3.3', 'P3.2', 'P3.1', 'P3.0']; pbot = ['P5.4', 'VCC', 'P5.5', 'GND']
for i, c_ in enumerate(range(6, 10)):
    text(X(c_), cy0 + 18, ptop[i], 12, '#9fd3ff'); text(X(c_), cy1 - 9, pbot[i], 12, '#9fd3ff')
RED, BLK, YEL, GRN, BLU, PUR, ORG, TEAL, BRN = '#e53935', '#2b2b2b', '#f2c200', '#2eab4f', '#1e88e5', '#8e44ad', '#ff8a00', '#00a3a3', '#a0522d'
# ---- jumpers on the board ----
wire(X(6), Y('a'), X(14), Y('a'), GRN, sag=-44)            # P3.3 -> CS
wire(X(7), Y('c'), X(15), Y('b'), YEL, sag=14)             # P3.2 -> SCK
wire(X(6), Y('j'), X(17), Y('j'), BLU, sag=70)             # P5.4 -> IO0
wire(X(8), Y('h'), X(16), Y('j'), PUR, sag=-8)             # P5.5 -> IO1
wire(X(9), Y('h'), X(16), Y('a'), BLK, sag=-190)           # chip GND -> module GND (passes between chip and module)
o.append(f'<rect x="{X(7)-9}" y="{Y("g")-12}" width="{2*P+18}" height="24" rx="12" fill="#d9a441" stroke="#8a6414"/>'); text(X(8), Y('g') + 6, '0.1µF', 14, '#3b2a05', weight=700)
# ---- USB-serial adapter ----
ax, ay, aw, ah = 60, Y('c') - 10, 200, 260
o.append(f'<rect x="{ax}" y="{ay}" width="{aw}" height="{ah}" rx="12" fill="#1d4e9e" stroke="#12356f" stroke-width="2"/>')
o.append(f'<rect x="{ax-34}" y="{ay+ah/2-40}" width="52" height="80" rx="5" fill="#c8ccd0" stroke="#7b8187"/>')
text(ax + aw / 2 - 20, ay + 38, 'USB 转串口', 20, '#fff', weight=700); text(ax + aw / 2 - 20, ay + 62, '(3.3V 电平)', 15, '#cfe0ff')
pins = [('5V', RED), ('TXD', TEAL), ('RXD', BRN), ('3V3', ORG), ('GND', BLK)]; py = {}
for i, (n, c) in enumerate(pins):
    y = ay + 100 + i * 34; py[n] = y
    o.append(f'<rect x="{ax+aw-20}" y="{y-8}" width="28" height="16" fill="#e8c34a" stroke="#8a6d12"/>'); text(ax + aw - 30, y + 6, n, 16, '#fff', anchor='end', weight=700)
ex = ax + aw + 8
wire(ex, py['5V'], X(17), Y('a'), RED, sag=-330, w=6)       # straight to the module's 5V column
wire(ex, py['TXD'], X(9), Y('b'), TEAL, sag=-70, w=6)       # adapter TXD -> P3.0
wire(ex, py['RXD'], X(8), Y('d'), BRN, sag=-10, w=6)        # adapter RXD -> P3.1
wire(ex, py['3V3'], X(7), Y('j'), ORG, sag=150, w=6)
wire(ex, py['GND'], X(9), Y('j'), BLK, sag=260, w=6)
text(X(1) - 10, Y('j') + P * 2.1, '下载时拔插橙色 3.3V 线', 16, '#c25e00', anchor='middle', weight=700)
# ---- legend ----
items = [(GRN, 'P3.3 → CS'), (YEL, 'P3.2 → SCK'), (BLU, 'P5.4 → IO0 (MOSI)'), (PUR, 'P5.5 → IO1 (D/C)'), (BLK, 'GND'),
         (TEAL, '串口 TXD → P3.0'), (BRN, '串口 RXD → P3.1'), (RED, '5V → 模块'), (ORG, '3.3V → 单片机')]
for i, (c, s) in enumerate(items):
    x = 70 + (i % 5) * 265; ly = H - 118 + (i // 5) * 32
    o.append(f'<line x1="{x}" y1="{ly}" x2="{x+38}" y2="{ly}" stroke="{c}" stroke-width="7" stroke-linecap="round"/>'); text(x + 50, ly + 6, s, 16, '#222', anchor='start')
text(70, H - 42, '板上跳线 5 根 + 串口模块引出 5 根 + 1 个 0.1µF 电容。模块的 RST、IO2、IO3、TE 不接。', 15, '#444', anchor='start')
text(70, H - 18, '示意图，未按此实际搭建验证；P3.3 用作片选在启用硬件 SPI 时是否正常，需上板确认。', 14, '#a33', anchor='start')
o.append('</svg>')
open('wiring.svg', 'w').write('\n'.join(o))
