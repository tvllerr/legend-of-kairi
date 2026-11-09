"""Sign a Legend of Kairi release zip (dev only, never shipped: mkzip SKIP_TOOLS). Run it with a python that has
libcrypto-3.dll next to it, e.g. the kit one:  D:/SoraBotW/sb/kit185/python/python.exe -I tools/installer/signrel.py ...

  keygen <keyfile>            make a new private key file (refuses to overwrite) and print the public key
  pub    <keyfile>            print the public key (goes into selfupdate.py PUBKEYS)
  sign   <keyfile> <zip>      write <zip>.sig (the release asset "Legend-of-Kairi-<ver>.zip.sig"); the zip must already have
                              its release name, the name is part of what is signed
  verify <zip> [pubkey hex]   check <zip>.sig against selfupdate.PUBKEYS (or the given key)

The key file is 64 hex characters. Keep it OUT of every git repo and keep a backup: without it no update can be signed,
and players then need one manual update to a release that carries a new public key."""
import hashlib, os, sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import sigcheck


def _priv(path):
    t = open(path, "r", encoding="ascii").read().strip()
    b = bytes.fromhex(t)
    if len(b) != 32:
        raise SystemExit("not a key file: " + path)
    return b


def _msg(zp):
    h = hashlib.sha256()
    with open(zp, "rb") as f:
        for b in iter(lambda: f.read(1 << 20), b""):
            h.update(b)
    return sigcheck.message(os.path.basename(zp), os.path.getsize(zp), h.hexdigest())


def main(a):
    if len(a) == 2 and a[0] == "keygen":
        if os.path.exists(a[1]):
            raise SystemExit("refusing to overwrite " + a[1])
        priv = os.urandom(32)
        os.makedirs(os.path.dirname(os.path.abspath(a[1])), exist_ok=True)
        with open(a[1], "x", encoding="ascii", newline="\n") as f:
            f.write(priv.hex() + "\n")
        print(sigcheck.public_of(priv).hex())
    elif len(a) == 2 and a[0] == "pub":
        print(sigcheck.public_of(_priv(a[1])).hex())
    elif len(a) == 3 and a[0] == "sign":
        priv = _priv(a[1])
        msg = _msg(a[2])
        sig = sigcheck.sign(priv, msg)
        if not sigcheck.verify(sigcheck.public_of(priv), sig, msg):
            raise SystemExit("self-check failed")
        with open(a[2] + ".sig", "w", encoding="ascii", newline="\n") as f:
            f.write(sig.hex() + "\n")
        print("wrote %s.sig (public key %s)" % (a[2], sigcheck.public_of(priv).hex()))
    elif len(a) in (2, 3) and a[0] == "verify":
        import selfupdate
        keys = [a[2]] if len(a) == 3 else selfupdate.PUBKEYS
        sig = bytes.fromhex(open(a[1] + ".sig", "r", encoding="ascii").read().strip())
        ok = any(sigcheck.verify(bytes.fromhex(k), sig, _msg(a[1])) for k in keys)
        print("GOOD signature" if ok else "BAD signature")
        return 0 if ok else 1
    else:
        print(__doc__)
        return 2
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
