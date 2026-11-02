r"""readme_images.py -- the pictures of README.md (docs\), from the icons the mod made: tools\test\run.cmd leaves them
as PNG in %TEMP%\fh2dualsense_test\DualSense\cache\png\<look>\<archive>\.

    python -I readme_images.py PNG_FOLDER OUT_FOLDER
"""
import os
import sys

from PIL import Image, ImageDraw, ImageFont

LOOKS = (('xbox', 'Xbox controller'), ('playstation', 'DualSense'), ('keyboard', 'Keyboard'))
BG, INK, DIM = (22, 26, 33, 255), (235, 238, 242, 255), (150, 158, 170, 255)


def font(size):
    for f in ('bahnschrift.ttf', 'segoeui.ttf', 'arial.ttf'):
        try:
            return ImageFont.truetype(os.path.join(os.environ.get('WINDIR', r'C:\Windows'), 'Fonts', f), size)
        except OSError:
            pass
    return ImageFont.load_default()


def icons(png, out):
    names = ['Controller_A', 'Controller_B', 'Controller_X', 'Controller_Y', 'Controller_LB', 'Controller_RB',
             'Controller_LT', 'Controller_RT', 'Controller_MENU', 'Controller_VIEW', 'Controller_DPadUpDown',
             'Controller_LeftThumb_LeftRight', 'Controller_RightThumb', 'Controller_L3']
    cell, left, top = 72, 250, 20
    im = Image.new('RGBA', (left + cell * len(names) + 20, top + cell * len(LOOKS) + 10), BG)
    d = ImageDraw.Draw(im)
    f = font(26)
    for y, (look, label) in enumerate(LOOKS):
        d.text((20, top + y * cell + cell // 2), label, font=f, fill=INK, anchor='lm')
        for x, n in enumerate(names):
            p = os.path.join(png, look, 'ControllerIcons', n + '.png')
            if os.path.exists(p):
                ic = Image.open(p).convert('RGBA').resize((cell - 8, cell - 8), Image.LANCZOS)
                im.alpha_composite(ic, (left + x * cell + 4, top + y * cell + 4))
    im.convert('RGB').save(os.path.join(out, 'icons.png'))


def controls(png, out):
    w = 512
    im = Image.new('RGBA', (w * 3, 430), BG)
    d = ImageDraw.Draw(im)
    f = font(28)
    for i, (look, label) in enumerate(LOOKS):
        dia = Image.open(os.path.join(png, look, 'UI_Symbols', 'Help_Controller.png')).convert('RGBA')
        im.alpha_composite(dia.crop((0, 100, 512, 470)).resize((w, 370)), (i * w, 0))
        d.text((i * w + w // 2, 398), label, font=f, fill=DIM, anchor='mm')
    im.convert('RGB').save(os.path.join(out, 'controls.png'))


if __name__ == '__main__':
    os.makedirs(sys.argv[2], exist_ok=True)
    icons(sys.argv[1], sys.argv[2])
    controls(sys.argv[1], sys.argv[2])
    print('made', os.path.join(sys.argv[2], 'icons.png'), os.path.join(sys.argv[2], 'controls.png'))
