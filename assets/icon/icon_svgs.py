"""icon_svgs.py [OUTDIR]: the app icon's SVG sources, a medallion ("ring of a thousand years") with a sword.

Variants: icon.svg (desktop master, transparent), icon-small.svg (16-48 px: no lettering or fine
pattern), android-foreground.svg / android-background.svg / android-monochrome.svg (108 dp layers).
build_icons.py renders them with rsvg-convert; the lettering uses Liberation Serif.
"""
import math, sys
from pathlib import Path

out = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(__file__).resolve().parent
out.mkdir(parents=True, exist_ok=True)
C = 512
TILT = 20  # sword angle, clockwise: upright reads as a power symbol, 45 degrees as a prohibition sign
SWORD = .74  # sword scale about the centre: it stays inside the lettered band, like a clock hand


def P(r, deg):
    a = math.radians(deg)
    return C + r * math.cos(a), C + r * math.sin(a)


DEFS = '''<defs>
<linearGradient id="rimHi" x1="0" y1="0" x2="1" y2="1"><stop offset="0" stop-color="#fff0c2"/><stop offset=".45" stop-color="#c8963f"/><stop offset="1" stop-color="#4a3010"/></linearGradient>
<linearGradient id="rimLo" x1="0" y1="0" x2="1" y2="1"><stop offset="0" stop-color="#5a3c14"/><stop offset=".55" stop-color="#a7782f"/><stop offset="1" stop-color="#f2d38c"/></linearGradient>
<linearGradient id="band" x1="0" y1="0" x2="1" y2="1"><stop offset="0" stop-color="#f7dc98"/><stop offset=".22" stop-color="#c99843"/><stop offset=".45" stop-color="#8a5f22"/><stop offset=".62" stop-color="#d0a24e"/><stop offset=".82" stop-color="#9a6b28"/><stop offset="1" stop-color="#e3bf72"/></linearGradient>
<radialGradient id="enamel" cx="46%" cy="40%" r="62%"><stop offset="0" stop-color="#3b78c2"/><stop offset=".55" stop-color="#1b4277"/><stop offset="1" stop-color="#0a1a33"/></radialGradient>
<radialGradient id="bg" cx="50%" cy="42%" r="75%"><stop offset="0" stop-color="#1f3a63"/><stop offset="1" stop-color="#08111f"/></radialGradient>
<linearGradient id="steel" x1="0" y1="0" x2="1" y2="0"><stop offset="0" stop-color="#ffffff"/><stop offset=".42" stop-color="#e2e6ee"/><stop offset=".52" stop-color="#b8bfcc"/><stop offset="1" stop-color="#6c7380"/></linearGradient>
<linearGradient id="goldH" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#fbe6ad"/><stop offset=".45" stop-color="#d4a54c"/><stop offset="1" stop-color="#7a521c"/></linearGradient>
<radialGradient id="goldBall" cx="38%" cy="32%" r="70%"><stop offset="0" stop-color="#fff3c8"/><stop offset=".45" stop-color="#d9aa4f"/><stop offset="1" stop-color="#6e4816"/></radialGradient>
<radialGradient id="gemBlue" cx="40%" cy="35%" r="70%"><stop offset="0" stop-color="#c9ecff"/><stop offset=".35" stop-color="#3f9be6"/><stop offset="1" stop-color="#0a2f63"/></radialGradient>
<radialGradient id="gemRed" cx="40%" cy="35%" r="70%"><stop offset="0" stop-color="#ffd0cc"/><stop offset=".4" stop-color="#d23a3a"/><stop offset="1" stop-color="#5a0d12"/></radialGradient>
<linearGradient id="leather" x1="0" y1="0" x2="1" y2="0"><stop offset="0" stop-color="#2a170a"/><stop offset=".45" stop-color="#5c3818"/><stop offset="1" stop-color="#1e1007"/></linearGradient>
<radialGradient id="gloss" cx="50%" cy="0%" r="100%"><stop offset="0" stop-color="#ffffff" stop-opacity=".22"/><stop offset="1" stop-color="#ffffff" stop-opacity="0"/></radialGradient>
<filter id="grain" x="0" y="0" width="100%" height="100%"><feTurbulence type="fractalNoise" baseFrequency=".85" numOctaves="3" seed="7" result="n"/><feColorMatrix in="n" type="matrix" values="0 0 0 0 0  0 0 0 0 0  0 0 0 0 0  .9 0 0 0 -.25" result="a"/><feComposite in="a" in2="SourceGraphic" operator="in"/></filter>
<filter id="soft" x="-30%" y="-30%" width="160%" height="160%"><feGaussianBlur stdDeviation="12"/></filter>
<filter id="softer" x="-30%" y="-30%" width="160%" height="160%"><feGaussianBlur stdDeviation="22"/></filter>
<clipPath id="enamelClip"><circle cx="512" cy="512" r="282"/></clipPath>
<clipPath id="gripClip"><rect x="492" y="676" width="40" height="110" rx="9"/></clipPath>
</defs>'''


def glyph_w(ch, size):
    if ch == ' ': return .32 * size
    if ch in 'MW': return .92 * size
    if ch in 'I': return .36 * size
    if ch in '·': return .3 * size
    return .7 * size


def arc_text(text, r, center_deg, size, top=True, spacing=.16):
    widths = [glyph_w(c, size) + spacing * size for c in text]
    total = sum(widths)
    span = total / r  # radians
    out = []
    pos = -total / 2
    for c, w in zip(text, widths):
        mid = pos + w / 2; pos += w
        if c == ' ': continue
        d = math.degrees(mid / r)
        deg = center_deg + d if top else center_deg - d
        x, y = P(r, deg)
        rot = deg + 90 if top else deg - 90
        cap = .33 * size
        t = (f'<g transform="translate({x:.1f},{y:.1f}) rotate({rot:.2f})">'
             f'<text x="1.6" y="{cap + 2:.1f}" text-anchor="middle" font-family="Liberation Serif" font-weight="bold" font-size="{size}" fill="#fbe3a3" opacity=".55">{c}</text>'
             f'<text x="0" y="{cap:.1f}" text-anchor="middle" font-family="Liberation Serif" font-weight="bold" font-size="{size}" fill="#3a240b">{c}</text></g>')
        out.append(t)
    return ''.join(out)


def numerals(r, size):
    roman = 'XII I II III IV V VI VII VIII IX X XI'.split()
    s = []
    for i, n in enumerate(roman):
        deg = -90 + i * 30
        x, y = P(r, deg)
        rot = deg - 90 if 0 < deg % 360 < 180 else deg + 90  # lower half upright
        s.append(f'<g transform="translate({x:.1f},{y:.1f}) rotate({rot:.1f})">'
                 f'<text x="0" y="{.35 * size + 2:.1f}" text-anchor="middle" font-family="Liberation Serif" font-weight="bold" font-size="{size}" fill="#061223" opacity=".6">{n}</text>'
                 f'<text x="0" y="{.35 * size:.1f}" text-anchor="middle" font-family="Liberation Serif" font-weight="bold" font-size="{size}" fill="#efd28a">{n}</text></g>')
    return ''.join(s)


def diamond(deg, r, size):
    x, y = P(r, deg)
    return (f'<g transform="translate({x:.1f},{y:.1f}) rotate({deg + 45:.1f})">'
            f'<rect x="{-size}" y="{-size}" width="{2 * size}" height="{2 * size}" fill="url(#goldBall)" stroke="#3a240b" stroke-width="3"/></g>')


def guilloche():
    s = ['<g clip-path="url(#enamelClip)">']
    s.append('<g fill="none" stroke="#ffffff" stroke-width="2" opacity=".06">' + ''.join(
        f'<circle cx="{C}" cy="{C}" r="{r}"/>' for r in range(26, 282, 10)) + '</g>')
    lines = []
    for i in range(120):
        x0, y0 = P(18, i * 3); x1, y1 = P(282, i * 3)
        lines.append(f'<line x1="{x0:.1f}" y1="{y0:.1f}" x2="{x1:.1f}" y2="{y1:.1f}"/>')
    s.append('<g stroke="#000000" stroke-width="2.4" opacity=".10">' + ''.join(lines) + '</g>')
    s.append('</g>')
    return ''.join(s)


def medallion(detail=True):
    m = []
    m.append(f'<circle cx="{C}" cy="{C + 18}" r="374" fill="#000000" opacity=".55" filter="url(#softer)"/>')
    m.append(f'<circle cx="{C}" cy="{C}" r="374" fill="url(#rimHi)"/>')
    m.append(f'<circle cx="{C}" cy="{C}" r="364" fill="url(#rimLo)"/>')
    m.append(f'<circle cx="{C}" cy="{C}" r="356" fill="url(#band)"/>')
    m.append(f'<circle cx="{C}" cy="{C}" r="356" fill="#000000" filter="url(#grain)" opacity=".5"/>')
    if detail:
        m.append(arc_text('LOST ODYSSEY', 323, -90, 50, top=True))
        m.append(arc_text('RECOMPILED', 323, 90, 50, top=False))
        m.append(diamond(0, 323, 13) + diamond(180, 323, 13))
        m.append(diamond(-38, 323, 6) + diamond(-142, 323, 6) + diamond(38, 323, 6) + diamond(142, 323, 6))
    m.append(f'<circle cx="{C}" cy="{C}" r="294" fill="url(#rimLo)"/>')
    m.append(f'<circle cx="{C}" cy="{C}" r="288" fill="url(#rimHi)"/>')
    m.append(f'<circle cx="{C}" cy="{C}" r="282" fill="url(#enamel)"/>')
    if detail:
        m.append(guilloche())
        m.append(numerals(246, 34))
        m.append(f'<circle cx="{C}" cy="{C}" r="216" fill="none" stroke="#e7c77d" stroke-width="5" opacity=".9"/>')
        m.append(f'<circle cx="{C}" cy="{C}" r="210" fill="none" stroke="#061223" stroke-width="3" opacity=".5"/>')
    m.append(f'<ellipse cx="{C}" cy="{C - 120}" rx="250" ry="170" fill="url(#gloss)" clip-path="url(#enamelClip)"/>')
    return ''.join(m)


def sword_shape(fill):
    """Flat silhouette (for the shadow and the monochrome icon), local frame: tip up at y=110."""
    return (f'<polygon points="512,110 548,198 544,632 480,632 476,198" fill="{fill}"/>'
            f'<path d="M380 642 C420 628 470 628 512 630 C554 628 604 628 644 642 L648 668 C604 656 554 662 512 664 C470 662 420 656 376 668 Z" fill="{fill}"/>'
            f'<circle cx="372" cy="655" r="19" fill="{fill}"/><circle cx="652" cy="655" r="19" fill="{fill}"/>'
            f'<rect x="482" y="620" width="60" height="58" rx="16" fill="{fill}"/>'
            f'<rect x="490" y="672" width="44" height="122" rx="10" fill="{fill}"/>'
            f'<circle cx="512" cy="826" r="36" fill="{fill}"/>')


def sword(detail=True):
    k = '#24170a'
    s = [f'<g stroke="{k}" stroke-width="4.5" stroke-linejoin="round">']
    s.append('<polygon points="512,110 548,198 544,632 480,632 476,198" fill="url(#steel)"/>')
    s.append('</g>')
    if detail:
        s.append('<polygon points="506,236 518,236 518,560 512,574 506,560" fill="#5f6672"/>')
        s.append('<line x1="505" y1="240" x2="505" y2="560" stroke="#ffffff" stroke-width="2.5" opacity=".75"/>')
        s.append('<polyline points="512,120 482,200 486,626" fill="none" stroke="#ffffff" stroke-width="3.5" opacity=".9"/>')
        s.append('<polyline points="512,120 542,200 538,626" fill="none" stroke="#3c414b" stroke-width="3" opacity=".7"/>')
        s.append('<line x1="512" y1="128" x2="512" y2="226" stroke="#9aa1ad" stroke-width="2" opacity=".8"/>')
    s.append(f'<g stroke="{k}" stroke-width="4.5" stroke-linejoin="round">')
    s.append('<path d="M380 642 C420 628 470 628 512 630 C554 628 604 628 644 642 L648 668 C604 656 554 662 512 664 C470 662 420 656 376 668 Z" fill="url(#goldH)"/>')
    s.append('<circle cx="372" cy="655" r="19" fill="url(#goldBall)"/><circle cx="652" cy="655" r="19" fill="url(#goldBall)"/>')
    s.append('<rect x="490" y="672" width="44" height="122" rx="10" fill="url(#leather)"/>')
    s.append('<rect x="482" y="620" width="60" height="58" rx="16" fill="url(#goldH)"/>')
    s.append('<rect x="486" y="670" width="52" height="12" rx="4" fill="url(#goldH)"/>')
    s.append('<rect x="486" y="784" width="52" height="12" rx="4" fill="url(#goldH)"/>')
    s.append('<circle cx="512" cy="826" r="36" fill="url(#goldBall)"/>')
    s.append('<circle cx="512" cy="649" r="18" fill="url(#gemBlue)"/>')
    s.append('</g>')
    if detail:
        wraps = ''.join(f'<line x1="486" y1="{y}" x2="538" y2="{y + 26}"/><line x1="538" y1="{y}" x2="486" y2="{y + 26}"/>'
                        for y in range(672, 800, 20))
        s.append(f'<g clip-path="url(#gripClip)" stroke="#8a5a2b" stroke-width="5" opacity=".85">{wraps}</g>')
        s.append('<g stroke="#ffffff" stroke-width="1.6" opacity=".45" fill="none">'
                 '<line x1="512" y1="633" x2="512" y2="665"/><line x1="496" y1="649" x2="528" y2="649"/>'
                 '<line x1="501" y1="638" x2="523" y2="660"/><line x1="523" y1="638" x2="501" y2="660"/></g>')
        s.append('<ellipse cx="506" cy="642" rx="6" ry="4" fill="#ffffff" opacity=".9"/>')
        s.append('<circle cx="512" cy="826" r="12" fill="url(#gemRed)" stroke="#24170a" stroke-width="3"/>')
        s.append('<ellipse cx="509" cy="822" rx="4" ry="3" fill="#ffffff" opacity=".85"/>')
        s.append('<path d="M392 640 C430 632 470 632 500 634" fill="none" stroke="#fff3cc" stroke-width="3" opacity=".7"/>')
        s.append('<ellipse cx="500" cy="812" rx="12" ry="7" fill="#ffffff" opacity=".35"/>')
    return ''.join(s)


def sword_frame():
    return f'rotate({TILT} {C} {C}) translate({C * (1 - SWORD):.1f},{C * (1 - SWORD):.1f}) scale({SWORD})'


def emblem(detail=True):
    rot = sword_frame()
    return (medallion(detail)
            + f'<g transform="translate(18,22) {rot}" filter="url(#soft)" opacity=".6">{sword_shape("#000000")}</g>'
            + f'<g transform="{rot}">{sword(detail)}</g>')


def svg(body, defs=True):
    return ('<svg xmlns="http://www.w3.org/2000/svg" width="1024" height="1024" viewBox="0 0 1024 1024">'
            + (DEFS if defs else '') + body + '</svg>\n')


def scaled(body, s):
    return f'<g transform="translate({C * (1 - s):.1f},{C * (1 - s):.1f}) scale({s})">{body}</g>'


(out / 'icon.svg').write_text(svg(scaled(emblem(True), 1.24)))
(out / 'icon-small.svg').write_text(svg(scaled(emblem(False), 1.28)))
(out / 'android-foreground.svg').write_text(svg(scaled(emblem(True), .78)))
stars = ''.join(f'<circle cx="{x}" cy="{y}" r="{r}" fill="#ffffff" opacity=".35"/>'
                for x, y, r in [(170, 230, 4), (860, 260, 5), (220, 850, 3), (820, 820, 4), (120, 560, 3), (900, 560, 3)])
(out / 'android-background.svg').write_text(svg('<rect width="1024" height="1024" fill="url(#bg)"/>' + stars))
cut = sword_shape('#000000')
mono = ('<defs><mask id="gap" maskUnits="userSpaceOnUse" x="0" y="0" width="1024" height="1024">'
        '<rect width="1024" height="1024" fill="#ffffff"/>'
        f'<g transform="{sword_frame()}" stroke="#000000" stroke-width="56" stroke-linejoin="round">{cut}</g></mask></defs>'
        + scaled(f'<circle cx="{C}" cy="{C}" r="330" fill="none" stroke="#ffffff" stroke-width="72" mask="url(#gap)"/>'
                 f'<circle cx="{C}" cy="{C}" r="250" fill="none" stroke="#ffffff" stroke-width="14" mask="url(#gap)"/>'
                 f'<g transform="{sword_frame()}">{sword_shape("#ffffff")}</g>', .78))
(out / 'android-monochrome.svg').write_text(svg(mono, defs=False))
print('wrote', sorted(p.name for p in out.glob('*.svg')))
