"""(s223) rebuild_tools: a copy of an existing kit with the C# tools whose sources changed rebuilt from this checkout - dev PC only
(mkzip SKIP_TOOLS). sorabuild refuses a kit tool built from other sources ("kit tool X was built from other sources ... rebuild
the kit"); this is the quick way to make the kit fit a release's tools/sorabuild, the way s217 rebuilt sorabake by hand.

  python -E -P tools/sorabuild/kit/rebuild_tools.py --base D:/SoraBotW/sb/kit185 --out <new empty dir>
      [--dotnet D:/Tools/dotnet8/dotnet.exe] [--openkh D:/KH2BusterSword/conv/out] [--tools partybake,hudbake]

Without --tools: every tool of mkkit.CS whose sb.src_hash differs from the base kit.json "src". Same build command as mkkit.
The base kit is checked against its kit.json first (as mkkit --refresh-host does); kit.json is written fresh.
"""
import argparse
import glob
import json
import os
import shutil
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import mkkit  # noqa: E402

sb = mkkit.sb


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--base", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--dotnet", default="D:/Tools/dotnet8/dotnet.exe")
    ap.add_argument("--openkh", default="D:/KH2BusterSword/conv/out")
    ap.add_argument("--tools", default="")
    a = ap.parse_args()
    base, out = os.path.abspath(a.base), os.path.abspath(a.out)
    if os.path.exists(out) and os.listdir(out):
        raise SystemExit("rebuild_tools: %s is not empty" % out)
    kj = json.load(open(os.path.join(base, "kit.json"), encoding="utf-8"))
    want = kj["files"]
    bad = [r for r in sorted(want) if not os.path.isfile(os.path.join(base, r)) or mkkit.sha256(os.path.join(base, r)) != want[r]]
    if bad:
        raise SystemExit("rebuild_tools: base kit files changed since it was made: %s" % ", ".join(bad[:5]))
    src = dict(kj["src"])
    tools = [t for t in a.tools.split(",") if t] or [t for t in mkkit.CS if sb.src_hash(t) != src.get(t)]
    for t in tools:
        if t not in mkkit.CS:
            raise SystemExit("rebuild_tools: %s is not one of mkkit's C# tools (%s)" % (t, ", ".join(mkkit.CS)))
    if not tools:
        raise SystemExit("rebuild_tools: every C# tool of %s already fits this checkout" % base)
    if os.path.isdir(out):
        os.rmdir(out)
    shutil.copytree(base, out, ignore=lambda d, names: ["kit.json"] if os.path.normcase(d) == os.path.normcase(base) else [])
    tmp = os.path.join(out, "_build")
    env = {"DOTNET_CLI_HOME": os.path.join(tmp, "home"), "DOTNET_CLI_TELEMETRY_OPTOUT": "1", "DOTNET_NOLOGO": "1"}
    for t in tools:
        rel, prop = mkkit.CS[t]
        s = os.path.join(tmp, t)
        shutil.copytree(os.path.join(mkkit.SBDIR, rel), s)
        proj = glob.glob(os.path.join(s, "*.csproj"))[0]
        dst = os.path.join(out, "tools", t)
        shutil.rmtree(dst)
        mkkit.run([a.dotnet, "build", proj, "-c", "Release", "-o", dst, "-p:%s=%s" % (prop, a.openkh.replace("\\", "/")),
                   "--nologo", "-v", "q"], cwd=s, env=env)
        src[t] = sb.src_hash(t)
    shutil.rmtree(tmp)
    stamp = time.strftime("%Y-%m-%d %H:%M:%S")
    mkkit.write_kit_json(out, src, {"base": {"kit": os.path.basename(base), "made": kj.get("made")},
                                    "rebuilt": list(kj.get("rebuilt") or []) + ["tools/%s (s223 rebuild_tools, %s)" % (t, stamp) for t in tools]})
    print("rebuild_tools: %s from %s, rebuilt %s" % (out, base, ", ".join(tools)))


if __name__ == "__main__":
    main()
