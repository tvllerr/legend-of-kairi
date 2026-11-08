# s116 mkface: bake KH2 King Mickey's HUD portrait into out_s116/mkface.bin.
# KH2 has no Mickey shopface (menu/shopface has no ex200), so this uses KH2's own Jiminy journal portrait
# menu/jmface_ex18 (256x256, king outfit = the playable P_EX200; jmface_ex17 = the Organization coat), upscaled to 384 like
# Sora's p_ex100 (hud.bin image 1). 'SKRF' v1: u32 magic, u32 ver, u32 w, u32 h, then RGBA8 (straight alpha).
import struct, sys
import os
KH2 = os.environ['SORABUILD_KH2'].rstrip('/')  # (sorabuild) the KH2 extract root, was D:/KH2Extract
from PIL import Image
src = KH2 + '/kh2_fifth/remastered/menu/jmface_ex18.imd/-0.dds'
out = sys.argv[1] if len(sys.argv) > 1 else 'out_s116/mkface.bin'
im = Image.open(src).convert('RGBA').resize((384, 384), Image.LANCZOS)
open(out, 'wb').write(b'SKRF' + struct.pack('<III', 1, im.width, im.height) + im.tobytes())
print(out, im.size)
