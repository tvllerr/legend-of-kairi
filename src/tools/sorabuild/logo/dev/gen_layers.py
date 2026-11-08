"""DEV ONLY (needs scipy; never run by sorabuild): regenerate ../kairi_layers.npz = our letters only (no BotW, no KH2 pixels).
  python gen_layers.py <old tools dir: tools_s110/logo of the sw-a41 history> <out dir with orig_logo_rgba.png, title_orig_rgba.png,
                        title_map.npy and kb/ (= `sorabake kbexport W_EX010 kb`)> <out npz>
The old scripts (build_logo.py, build_title.py, letters5.py, kb_layer.py ...) run unchanged; this only exec's them up to the
Keyblade step and saves the letter layers: La = chipped cream letters, Fr = the part of the letters in front of the Keyblade.
The old build writes out_s110/logo/*.png beside the tools dir (../../out_s110/logo): copy the tools to <x>/tools_s110/logo and
point the out dir to <x>/out_s110/logo."""
import os
import sys

tools, outdir, npz = (os.path.abspath(p) for p in sys.argv[1:4])
sys.path.insert(0, tools)
os.chdir(tools)
import numpy as np  # noqa: E402

kb = os.path.join(outdir, 'kb')
sys.argv = ['build_title.py', kb]
src = open('build_title.py', encoding='utf-8').read()
ns = {'__file__': os.path.join(tools, 'build_title.py'), '__name__': 'bt'}
exec(compile(src[:src.index("over(fc, fa * (1 - Fr))")], 'build_title.py', 'exec'), ns)
s2 = open('build_logo.py', encoding='utf-8').read()
n2 = {'__file__': os.path.join(tools, 'build_logo.py'), '__name__': 'bl'}
exec(compile(s2[:s2.index('# Keyblade (unchanged)')], 'build_logo.py', 'exec'), n2)
np.savez_compressed(npz, cut_La=n2['La'], cut_Fr=n2['Fr'], title_La=ns['La'], title_Fr=ns['Fr'])
print('wrote', npz)
