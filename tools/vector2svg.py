#!/usr/bin/env python3
"""Converts Android vector drawables to SVG icons for GTK.

  vector2svg.py src.xml=dst-symbolic.svg ...   single colour (GTK recolours symbolic icons)
  vector2svg.py --color src.xml=dst.svg ...    keeps the original colours
"""
import sys
import xml.etree.ElementTree as ET

A = '{http://schemas.android.com/apk/res/android}'


def attr(el, name, default=None):
    return el.get(A + name, default)


def color(c):
    """#RGB, #ARGB, #RRGGBB, #AARRGGBB -> (css colour, opacity)."""
    if not c or not c.startswith('#'):
        return '#000', 1.0
    h = c[1:]
    if len(h) in (3, 4):
        h = ''.join(ch * 2 for ch in h)
    if len(h) == 8:
        return '#' + h[2:], int(h[:2], 16) / 255.0
    return '#' + h, 1.0


def convert_layer_list(root, dst):
    """<layer-list> with a coloured oval and a vector on top (Android category icons)."""
    items = root.findall('item')
    size, circle, inner = 40.0, None, None
    for item in items:
        shape = item.find('shape')
        if shape is not None:
            sz = shape.find('size')
            if sz is not None:
                size = float(attr(sz, 'width', '40dp').rstrip('dp'))
            solid = shape.find('solid')
            if solid is not None:
                circle = color(attr(solid, 'color'))[0]
        vec = item.find('vector')
        if vec is not None:
            inner = vec
    tmp = dst + '.inner.svg'
    ET.ElementTree(inner).write(tmp)
    convert(tmp, tmp, True, force_white=True)
    import os, re
    body = open(tmp).read()
    os.remove(tmp)
    m = re.search(r'viewBox="0 0 ([\d.]+) ([\d.]+)"', body)
    vw = float(m.group(1))
    inner_size = float(attr(inner, 'width', '24dp').rstrip('dp'))
    off = (size - inner_size) / 2
    scale = inner_size / vw
    content = body[body.index('>') + 1:body.rindex('</svg>')]
    out = [f'<svg xmlns="http://www.w3.org/2000/svg" width="{size:g}" height="{size:g}" viewBox="0 0 {size:g} {size:g}">']
    if circle:
        out.append(f'<circle cx="{size/2:g}" cy="{size/2:g}" r="{size/2:g}" fill="{circle}"/>')
    out.append(f'<g transform="translate({off:g} {off:g}) scale({scale:g})">{content}</g>')
    out.append('</svg>')
    open(dst, 'w').write('\n'.join(out) + '\n')


def convert(src, dst, keep_colors, force_white=False):
    root = ET.parse(src).getroot()
    if root.tag == 'layer-list':
        convert_layer_list(root, dst)
        return
    vw = float(attr(root, 'viewportWidth', '24'))
    vh = float(attr(root, 'viewportHeight', '24'))
    size = 16 if not keep_colors else int(float(attr(root, 'width', '24dp').rstrip('dp')))
    out = [f'<svg xmlns="http://www.w3.org/2000/svg" width="{size}" height="{size}" viewBox="0 0 {vw:g} {vh:g}">']
    clip_id = [0]

    def path_el(child):
        d = attr(child, 'pathData')
        if not d:
            return
        fill_rule = 'evenodd' if attr(child, 'fillType') == 'evenOdd' else 'nonzero'
        fill_c, stroke_c = attr(child, 'fillColor'), attr(child, 'strokeColor')
        parts = [f'd="{d}"', f'fill-rule="{fill_rule}"']
        if keep_colors:
            if force_white and fill_c:
                parts.append('fill="#fff"')
            elif fill_c and not fill_c.startswith('@'):
                c, a = color(fill_c)
                a *= float(attr(child, 'fillAlpha', '1'))
                parts.append(f'fill="{c}"' + (f' fill-opacity="{a:.3f}"' if a < 1 else ''))
            elif fill_c:
                parts.append('fill="#fff"')
            else:
                parts.append('fill="none"')
            if stroke_c:
                c, a = color(stroke_c)
                a *= float(attr(child, 'strokeAlpha', '1'))
                parts.append(f'stroke="{c}" stroke-width="{attr(child, "strokeWidth", "1")}"')
                if a < 1:
                    parts.append(f'stroke-opacity="{a:.3f}"')
                cap, join = attr(child, 'strokeLineCap'), attr(child, 'strokeLineJoin')
                if cap:
                    parts.append(f'stroke-linecap="{cap}"')
                if join:
                    parts.append(f'stroke-linejoin="{join}"')
        else:
            if stroke_c and not fill_c:
                parts.append(f'fill="none" stroke="#000" stroke-width="{attr(child, "strokeWidth", "1")}"')
            else:
                a = attr(child, 'fillAlpha')
                parts.append('fill="#000"' + (f' fill-opacity="{a}"' if a else ''))
        out.append('<path ' + ' '.join(parts) + '/>')

    def walk(el):
        for child in el:
            tag = child.tag
            if tag == 'group':
                t = []
                tx, ty = attr(child, 'translateX', '0'), attr(child, 'translateY', '0')
                if tx != '0' or ty != '0':
                    t.append(f'translate({tx} {ty})')
                px, py = attr(child, 'pivotX', '0'), attr(child, 'pivotY', '0')
                rot = attr(child, 'rotation')
                if rot:
                    t.append(f'rotate({rot} {px} {py})')
                sx, sy = attr(child, 'scaleX'), attr(child, 'scaleY')
                if sx or sy:
                    t.append(f'translate({px} {py}) scale({sx or 1} {sy or 1}) translate(-{px} -{py})')
                clip = child.find('clip-path')
                extra = ''
                if clip is not None:
                    clip_id[0] += 1
                    cid = f'c{clip_id[0]}'
                    out.append(f'<clipPath id="{cid}"><path d="{attr(clip, "pathData")}"/></clipPath>')
                    extra = f' clip-path="url(#{cid})"'
                tr = f' transform="{" ".join(t)}"' if t else ''
                out.append(f'<g{tr}{extra}>')
                walk(child)
                out.append('</g>')
            elif tag == 'path':
                path_el(child)

    walk(root)
    out.append('</svg>')
    with open(dst, 'w') as f:
        f.write('\n'.join(out) + '\n')


if __name__ == '__main__':
    keep = False
    for arg in sys.argv[1:]:
        if arg == '--color':
            keep = True
            continue
        src, dst = arg.split('=', 1)
        convert(src, dst, keep)
