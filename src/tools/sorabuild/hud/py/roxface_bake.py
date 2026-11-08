# s111 roxhud: bake KH2 Roxas' HUD portrait (menu/shopface/p_ex110, the PC HD 384x384) into out_s111/roxface.bin
# 'SKRF' v1: u32 magic, u32 ver, u32 w, u32 h, then RGBA8 (straight, as hud.bin image 1 = p_ex100)
import struct, sys
import os
KH2 = os.environ['SORABUILD_KH2'].rstrip('/')  # (sorabuild) the KH2 extract root, was D:/KH2Extract
from PIL import Image
src = KH2 + '/kh2_fifth/remastered/menu/shopface/p_ex110.bin/-0.dds'
out = sys.argv[1] if len(sys.argv) > 1 else 'out_s111/roxface.bin'
im = Image.open(src).convert('RGBA')
open(out, 'wb').write(b'SKRF' + struct.pack('<III', 1, im.width, im.height) + im.tobytes())
print(out, im.size)
