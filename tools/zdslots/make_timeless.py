"""Builds images/gaunttimelessbutton.png: the Survival button with its label replaced by TIMELESS,
written in the game's own NativeAlien font. Three cells: normal, hover, selected. Needs Pillow."""
import os, re
from PIL import Image, ImageDraw, ImageFilter

GAME = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
IMG = os.path.join(GAME, 'images')
FONT = os.path.join(GAME, 'fonts')
TEXT = 'TIMELESS'
LOW = 'TIMELESS'


def read_font():
    txt = open(os.path.join(FONT, 'nativealien48.txt'), encoding='latin-1').read()
    chars = re.findall(r"'(.)'|\"(.)\"", txt[txt.index('CharList'):txt.index('WidthList')])
    chars = [a or b for a, b in chars]
    block = txt[txt.index('WidthList'):txt.index('RectList')]
    widths = [int(v) for v in re.findall(r'\d+', block)]
    block = txt[txt.index('RectList'):]
    block = block[:block.index(';')]
    rects = [tuple(int(v) for v in m) for m in re.findall(r'\(\s*(\d+),\s*(\d+),\s*(\d+),\s*(\d+)\)', block)]
    mask = Image.open(os.path.join(FONT, '_NativeAlien48.gif')).convert('L')
    return {c: (w, r) for c, w, r in zip(chars, widths, rects)}, mask


def draw_m(height):
    """the font's M turns into a solid block at button size, so draw an open one"""
    S = 4
    w, h = 40 * S, height * S
    stem = 8 * S
    im = Image.new('L', (w, h))
    d = ImageDraw.Draw(im)
    d.rectangle((0, 0, stem, h), fill=255)
    d.rectangle((w - stem, 0, w, h), fill=255)
    mid = w // 2
    low = int(h * 0.62)
    d.polygon([(0, 0), (stem, 0), (mid, low - stem), (w - stem, 0), (w, 0), (mid + stem // 2, low + stem // 2), (mid - stem // 2, low + stem // 2)], fill=255)
    return im.resize((40, height), Image.LANCZOS)


def label(font, mask, height):
    """white-on-black mask of the word, scaled to the wanted height"""
    glyphs = [font[c] for c in LOW]
    rh = max(g[1][3] for g in glyphs)
    w = sum(g[0] + 9 for g in glyphs) + 8
    out = Image.new('L', (w, rh))
    # cap height of the font, taken from the T
    tx, ty, tw, th = font['T'][1]
    box = mask.crop((tx, ty, tx + tw, ty + th)).getbbox()
    top, cap = box[1], box[3] - box[1]
    x = 2
    for ch, (adv, (rx, ry, rw, rh2)) in zip(LOW, glyphs):
        if ch == 'M':
            out.paste(draw_m(cap), (x, top))
            x += 40 + 9
            continue
        g = mask.crop((rx, ry, rx + rw, ry + rh2))
        out.paste(Image.composite(Image.new('L', g.size, 255), out.crop((x, 0, x + rw, rh2)), g), (x, 0))
        x += adv + 9
    out = out.crop(out.getbbox())
    return out.resize((max(1, out.width * height // out.height), height), Image.LANCZOS)


def erase(cell, x0, x1):
    """wipe the old label by blending the bare stone on its two sides, row by row"""
    px = cell.load()
    for y in range(cell.height):
        a, b = px[x0 - 1, y], px[x1, y]
        for x in range(x0, x1):
            t = (x - x0) / (x1 - x0)
            px[x, y] = tuple(int(a[i] + (b[i] - a[i]) * t) for i in range(3))
    # soften the row-by-row streaks the blend leaves behind
    box = (x0, 0, x1, cell.height)
    cell.paste(cell.crop(box).filter(ImageFilter.GaussianBlur(1.2)), box)


def draw(cell, word, cx, top, bottom):
    h = word.height
    x = cx - word.width // 2
    y = (cell.height - h) // 2
    # dark engraved shadow, then a vertical gradient like the other labels
    cell.paste((45, 18, 8), (x + 1, y + 1), word)
    grad = Image.new('RGB', word.size)
    for j in range(h):
        t = j / max(1, h - 1)
        c = tuple(int(top[i] + (bottom[i] - top[i]) * t) for i in range(3))
        grad.paste(c, (0, j, word.width, j + 1))
    cell.paste(grad, (x, y), word)


# the button sits on the stone ledge under the statue's feet, so it is smaller than the others
CELL_W, CELL_H = 108, 22


def main():
    src = Image.open(os.path.join(IMG, 'gauntsurvivalbutton.jpg')).convert('RGB')
    alpha = Image.open(os.path.join(IMG, '_gauntsurvivalbutton.gif')).convert('L')
    font, mask = read_font()
    cw = src.width // 3
    out = Image.new('RGB', (CELL_W * 3, CELL_H))
    outa = Image.new('L', (CELL_W * 3, CELL_H))
    # top colour, bottom colour, area of the old label, width of the new one
    styles = [((255, 205, 70), (205, 115, 10), 24, 100, 70),    # normal
              ((120, 255, 80), (20, 190, 30), 24, 100, 70),     # hover
              ((255, 240, 150), (235, 190, 80), 27, 96, 50)]    # selected (spirals stay)
    for i, (top, bottom, x0, x1, tw) in enumerate(styles):
        box = (i * cw, 0, (i + 1) * cw, src.height)
        cell = src.crop(box)
        erase(cell, x0, x1)
        cell = cell.resize((CELL_W, CELL_H), Image.LANCZOS).filter(ImageFilter.UnsharpMask(1, 60, 2))
        word = label(font, mask, 14).resize((tw, 14), Image.LANCZOS)
        draw(cell, word, CELL_W // 2 - (2 if i == 2 else 0), top, bottom)
        out.paste(cell, (i * CELL_W, 0))
        outa.paste(alpha.crop(box).resize((CELL_W, CELL_H), Image.LANCZOS), (i * CELL_W, 0))
    out.putalpha(outa)
    out.save(os.path.join(IMG, 'gaunttimelessbutton.png'))
    print('ok', out.size)


if __name__ == '__main__':
    main()
