#!/usr/bin/env python3
"""(s137 fps) Appends the Firecracker object's effects (P_EX360 elements 0..5 = fuse x4, burst-in-air 4, burst-on-ground 5) to the LIVE fx.bin
as clf_fx_0 .. clf_fx_5, by reusing tools_s136/chickfx.py's encoder (same format, old rows byte-identical, verified by its verify_fx).
   python -I tools_s137fps/fpsfx.py --fx-in <fx.bin> --out <dir>
Source of the frames = the ProjectHeartHunters effect recording cache (kh2_fx.bin / kh2_fx.json: recordings of KH2's own P_EX360.a.us effects).
That is DATA, not logic; the recording is the only effect source the mod's pipeline has (same as the s136 cl_fx_* elements)."""
import argparse, importlib.util, os, sys
HERE = os.path.dirname(os.path.abspath(__file__))
spec = importlib.util.spec_from_file_location("chickfx", os.path.join(HERE, "chickfx.py"))
cf = importlib.util.module_from_spec(spec); spec.loader.exec_module(cf)
cf.FX_PREFIX = "clf_fx_"; cf.FX_SRC_FMT = "ex360.%d"; cf.FX_ELEMENTS = list(range(0, 6))
ap = argparse.ArgumentParser()
ap.add_argument("--fx-in", required=True)
ap.add_argument("--out", required=True)
a = ap.parse_args()
os.makedirs(a.out, exist_ok=True)
rep = cf.build_fx(a.fx_in, os.path.join(a.out, "fx.bin"), 0)
for r in rep: print(r)
