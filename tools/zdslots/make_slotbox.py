"""Build images/slotbox.jpg + images/_slotbox.gif: the dialog box at half size (thinner frame for save slots)."""
import sys, os
from PIL import Image
g = sys.argv[1]
im = Image.open(os.path.join(g, 'images/dialogbox.jpg')).convert('RGB')
a = Image.open(os.path.join(g, 'images/_dialogbox.gif')).convert('L')
w, h = im.width // 2, im.height // 2
im.resize((w, h), Image.LANCZOS).save(os.path.join(g, 'images/slotbox.jpg'), quality=95)
a.resize((w, h), Image.LANCZOS).convert('P').save(os.path.join(g, 'images/_slotbox.gif'))
print('ok', w, h)
