"""Builds the sprites for the special balls (run from the game folder or anywhere):
  images/zdx_rainbow.png   32x32 rainbow ball (drawn over the normal ball, rotated with it)
  images/zdx_skull.png     icon drawn on top of a skull ball
  images/zdx_frog.png      icon drawn on top of a frog ball
  images/zdx_minifrog.png  the little frog that runs out of the skull hole
Needs Pillow."""
import colorsys, math, os
from PIL import Image, ImageDraw, ImageFilter

GAME = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
IMG = os.path.join(GAME, 'images')


def load(name, alpha):
    im = Image.open(os.path.join(IMG, name)).convert('RGB')
    a = Image.open(os.path.join(IMG, alpha)).convert('L')
    im = im.crop((0, 0, a.width, a.height)) if im.height > a.height else im
    im.putalpha(a)
    return im


def rainbow():
    base = load('bWhite.gif', '_b.gif')           # first frame of the white ball
    out = Image.new('RGBA', base.size)
    w, h = base.size
    cx, cy = (w - 1) / 2, (h - 1) / 2
    for y in range(h):
        for x in range(w):
            r, g, b, a = base.getpixel((x, y))
            if not a:
                continue
            lum = (0.3 * r + 0.59 * g + 0.11 * b) / 255
            ang = math.atan2(y - cy, x - cx)
            d = math.hypot(x - cx, y - cy) / (w / 2)
            hue = (ang / (2 * math.pi) + d * 0.35) % 1.0     # swirl
            rr, gg, bb = colorsys.hsv_to_rgb(hue, 0.85, 1.0)
            k = min(1.0, lum * 1.25)
            hi = max(0.0, lum - 0.8) * 3                      # keep the white highlight
            out.putpixel((x, y), tuple(int(min(255, (c * k + hi) * 255)) for c in (rr, gg, bb)) + (a,))
    return out


def disc(size, r, alpha=190):
    im = Image.new('RGBA', (size, size))
    d = ImageDraw.Draw(im)
    c = size / 2
    d.ellipse((c - r, c - r, c + r, c + r), fill=(20, 16, 10, alpha))
    return im


def skull_icon():
    S = 4                                   # draw big, scale down for anti-aliasing
    n = 22 * S
    im = disc(n, 10.5 * S)
    d = ImageDraw.Draw(im)
    W = (245, 240, 225, 255)
    c = n / 2
    # cranium + jaw
    d.ellipse((c - 7 * S, c - 8 * S, c + 7 * S, c + 4 * S), fill=W)
    d.rounded_rectangle((c - 4.5 * S, c + 1 * S, c + 4.5 * S, c + 7.5 * S), radius=1.5 * S, fill=W)
    # eyes, nose
    K = (20, 16, 10, 255)
    d.ellipse((c - 5.5 * S, c - 3.5 * S, c - 1.2 * S, c + 1.2 * S), fill=K)
    d.ellipse((c + 1.2 * S, c - 3.5 * S, c + 5.5 * S, c + 1.2 * S), fill=K)
    d.polygon([(c, c + 1.5 * S), (c - 1.3 * S, c + 3.6 * S), (c + 1.3 * S, c + 3.6 * S)], fill=K)
    # teeth
    for i in (-1.5, 0, 1.5):
        d.line((c + i * S * 1.2, c + 5 * S, c + i * S * 1.2, c + 7.5 * S), fill=K, width=int(0.8 * S))
    return im.resize((22, 22), Image.LANCZOS)


def frog_icon():
    frog = load('toadlives.gif', '_toadlives.gif')
    im = disc(22 * 4, 10.5 * 4).resize((22, 22), Image.LANCZOS)
    f = frog.resize((19, 18), Image.LANCZOS)
    im.alpha_composite(f, (2, 2))
    return im


def minifrog():
    frog = load('toadlives.gif', '_toadlives.gif')
    big = frog.resize((36, 34), Image.LANCZOS)
    # soft dark outline so it reads on any background
    a = big.getchannel('A').filter(ImageFilter.MaxFilter(3))
    sh = Image.new('RGBA', big.size, (10, 25, 5, 0))
    sh.putalpha(a.point(lambda v: v * 200 // 255))
    out = Image.new('RGBA', (38, 36))
    out.alpha_composite(sh, (1, 1))
    out.alpha_composite(big, (1, 1))
    return out


if __name__ == '__main__':
    rainbow().save(os.path.join(IMG, 'zdx_rainbow.png'))
    skull_icon().save(os.path.join(IMG, 'zdx_skull.png'))
    frog_icon().save(os.path.join(IMG, 'zdx_frog.png'))
    minifrog().save(os.path.join(IMG, 'zdx_minifrog.png'))
    print('ok')
