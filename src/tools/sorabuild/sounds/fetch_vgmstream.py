#!/usr/bin/env python3
"""Fetch a pinned vgmstream-cli (ISC license, github.com/vgmstream/vgmstream) into
tools/sorabuild/sounds/bin/ for the Ogg Vorbis voices (build_sounds.py calls it).

User approved the download in s155/s156.  Pinned: official release r2117 (2026-05-19), asset vgmstream-win64.zip;
sha256 as listed by GitHub for that asset.  The sha256 is checked BEFORE unpacking; only vgmstream-cli.exe and its DLLs
are unpacked.  sounds/bin/ is gitignored (the binary is never committed).

  python -E -P fetch_vgmstream.py
"""
import hashlib
import io
import os
import sys
import urllib.request
import zipfile

PIN_TAG = "r2117"     # pinned, never "latest"
SHA256 = "6c4a8a3813864fefed081bbd337dbc0ad93bf88e0b92f5db98d7ab258b22dc6c"  # vgmstream-win64.zip r2117
URL = "https://github.com/vgmstream/vgmstream/releases/download/%s/vgmstream-win64.zip" % PIN_TAG
DEST = os.path.join(os.path.dirname(os.path.abspath(__file__)), "bin")


def main():
    if "TODO" in (PIN_TAG, SHA256):
        sys.exit("fetch_vgmstream: PIN_TAG / SHA256 are still TODO - ask the user to approve a version first")
    data = urllib.request.urlopen(URL, timeout=120).read()
    got = hashlib.sha256(data).hexdigest()
    if got != SHA256:
        sys.exit("fetch_vgmstream: sha256 mismatch (got %s)" % got)
    os.makedirs(DEST, exist_ok=True)
    with zipfile.ZipFile(io.BytesIO(data)) as z:
        for n in z.namelist():
            base = os.path.basename(n)
            if base == "vgmstream-cli.exe" or base.lower().endswith(".dll"):
                with open(os.path.join(DEST, base), "wb") as f:
                    f.write(z.read(n))
    print("vgmstream-cli unpacked to", DEST)


if __name__ == "__main__":
    main()
