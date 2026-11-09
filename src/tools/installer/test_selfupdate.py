"""Tests for selfupdate.py (no network: releases JSON + zips served via file://). Run: python tools/installer/test_selfupdate.py"""
import hashlib, io, json, os, pathlib, shutil, subprocess, sys, unittest, zipfile

HERE = os.path.dirname(os.path.abspath(__file__))
SU = os.path.join(HERE, "selfupdate.py")
sys.path.insert(0, HERE)
import sigcheck
PRIV = bytes(range(32))                 # a fixed test key, never a real one
PUB = sigcheck.public_of(PRIV).hex()
TMP = "D:/SoraBotW/sb/tmp184"
TOP = "Legend of Kairi 0.1.9-alpha"


def make_zip(path, ver="0.1.9-alpha", extra=None, release="Legend of Kairi"):
    top = "Legend of Kairi " + ver
    with zipfile.ZipFile(path, "w") as z:
        z.writestr(top + "/manifest.json", json.dumps({"release": release, "version": ver}))
        z.writestr(top + "/tools/installer/installer.py", "print(1)")
        z.writestr(top + "/kit/python/pythonw.exe", "x")
        for n in extra or []:
            z.writestr(n, "evil")


class T(unittest.TestCase):
    def setUp(self):
        shutil.rmtree(TMP, ignore_errors=True)
        self.root = os.path.join(TMP, "inst")
        os.makedirs(os.path.join(self.root, "data"))
        os.makedirs(os.path.join(self.root, "work"))
        pathlib.Path(self.root, "manifest.json").write_text(json.dumps({"release": "Legend of Kairi", "version": "0.1.2-alpha"}))
        self.srv = os.path.join(TMP, "srv")
        os.makedirs(self.srv)

    def tearDown(self):
        shutil.rmtree(TMP, ignore_errors=True)

    def release(self, ver, zp=None, digest="auto", draft=False, pre=False, sig="auto"):
        name = "Legend-of-Kairi-%s.zip" % ver
        zp = zp or os.path.join(self.srv, name)
        if not os.path.exists(zp):
            make_zip(zp, ver)
        data = open(zp, "rb").read()
        a = {"name": name, "size": len(data), "browser_download_url": pathlib.Path(zp).as_uri()}
        if digest == "auto":
            a["digest"] = "sha256:" + hashlib.sha256(data).hexdigest()
        elif digest:
            a["digest"] = digest
        assets = [a]
        if sig:
            # sig: "auto" = good; "wrongkey" = signed by somebody else; "othername" = a good signature of another release name;
            # "garbage" = not a signature at all
            msg = sigcheck.message("Legend-of-Kairi-0.0.1.zip" if sig == "othername" else name, len(data),
                                   a["digest"].split(":")[1] if a.get("digest") else hashlib.sha256(data).hexdigest())
            text = "hello" if sig == "garbage" else sigcheck.sign(bytes(32) if sig == "wrongkey" else PRIV, msg).hex()
            sp = zp + ".sig"
            with open(sp, "w") as f:
                f.write(text + "\n")
            assets.append({"name": name + ".sig", "size": os.path.getsize(sp), "browser_download_url": pathlib.Path(sp).as_uri()})
        return {"tag_name": "v" + ver, "draft": draft, "prerelease": pre, "assets": assets}

    def run_su(self, rels, *args):
        api = os.path.join(self.srv, "rel.json")
        json.dump(rels, open(api, "w"))
        p = subprocess.run([sys.executable, "-I", SU, "--root", self.root, "--api", pathlib.Path(api).as_uri(),
                            *([] if "--pubkey" in args else ["--pubkey", PUB]), *args],
                           capture_output=True, text=True, input="")
        return p.returncode, p.stdout + p.stderr

    def dl(self):
        return os.path.join(self.root, ".kairi-download")

    def test_newer_extracted(self):
        rc, out = self.run_su([self.release("0.1.9-alpha"), self.release("0.1.3-alpha")], "--yes", "--no-launch")
        self.assertEqual(rc, 0, out)
        new = os.path.join(self.dl(), "n")          # (s184 review) short folder, the zip's top folder dropped
        self.assertIn(os.path.normpath(new), out)
        self.assertTrue(os.path.isfile(os.path.join(new, "manifest.json")))
        self.assertFalse(os.path.exists(os.path.join(self.dl(), "Legend-of-Kairi-0.1.9-alpha.zip")))

    def test_check_writes_nothing(self):
        rc, out = self.run_su([self.release("0.1.9-alpha")], "--check")
        self.assertEqual(rc, 0, out)
        self.assertFalse(os.path.exists(self.dl()))

    def test_up_to_date(self):
        rc, out = self.run_su([self.release("0.1.2-alpha")], "--yes", "--no-launch")
        self.assertEqual(rc, 2, out)
        self.assertIn("up to date", out)
        rc, out = self.run_su([self.release("0.1.2-alpha")], "--yes", "--no-launch", "--force")
        self.assertEqual(rc, 0, out)

    def test_unfinished_update_refetches(self):
        pathlib.Path(self.root, "manifest.json").write_text(json.dumps({"release": "Legend of Kairi", "version": "0.1.9-alpha"}))
        os.makedirs(os.path.join(self.root, ".kairi-update"))
        pathlib.Path(self.root, ".kairi-update", "state.json").write_text("{}")
        rc, out = self.run_su([self.release("0.1.9-alpha")], "--yes", "--no-launch")
        self.assertEqual(rc, 0, out)
        self.assertIn("did not finish", out)
        rc, out = self.run_su([self.release("0.1.9-alpha")], "--check")     # --check still only reports
        self.assertEqual(rc, 2, out)

    def test_never_installed_refused(self):
        shutil.rmtree(os.path.join(self.root, "work"))
        rc, out = self.run_su([self.release("0.1.9-alpha")], "--yes", "--no-launch")
        self.assertEqual(rc, 1, out)
        self.assertIn("never installed", out)

    def test_top_folder_dropped_and_exact_asset(self):
        r = self.release("0.1.9-alpha")
        src = dict(r["assets"][0], name="Legend-of-Kairi-0.1.9-alpha-src.zip")      # listed first, must not be taken
        r["assets"].insert(0, src)
        rc, out = self.run_su([r], "--yes", "--no-launch")
        self.assertEqual(rc, 0, out)
        new = os.path.join(self.root, ".kairi-download", "n")
        self.assertTrue(os.path.isfile(os.path.join(new, "manifest.json")), out)
        self.assertEqual(os.path.normcase(out.strip().splitlines()[-1]), os.path.normcase(new))

    def test_bad_digest(self):
        rc, out = self.run_su([self.release("0.1.9-alpha", digest="sha256:" + "0" * 64)], "--yes", "--no-launch")
        self.assertEqual(rc, 1, out)
        self.assertIn("checksum", out)
        self.assertFalse(os.path.exists(os.path.join(self.dl(), "Legend-of-Kairi-0.1.9-alpha")))
        self.assertEqual([n for n in os.listdir(self.dl())], [])

    def test_unsigned_skipped(self):
        rc, out = self.run_su([self.release("0.1.9-alpha", sig=None)], "--yes", "--no-launch")
        self.assertEqual(rc, 1, out)
        self.assertIn("not signed", out)
        self.assertFalse(os.path.exists(self.dl()))
        # an unsigned newer release never hides a signed older one
        rc, out = self.run_su([self.release("0.2.0-alpha", sig=None), self.release("0.1.9-alpha")], "--yes", "--no-launch")
        self.assertEqual(rc, 0, out)
        self.assertEqual(json.load(open(os.path.join(self.dl(), "n", "manifest.json")))["version"], "0.1.9-alpha")

    def test_bad_signatures_refused(self):
        for kind in ("wrongkey", "othername", "garbage"):
            rc, out = self.run_su([self.release("0.1.9-alpha", sig=kind)], "--yes", "--no-launch")
            self.assertEqual(rc, 1, (kind, out))
            self.assertIn("not signed by", out, kind)
            self.assertFalse(os.path.exists(self.dl()), kind)        # refused before anything is downloaded
        # a good test-key signature is refused when the updater checks against another key
        rc, out = self.run_su([self.release("0.1.9-alpha")], "--yes", "--no-launch", "--pubkey", "00" * 32)
        self.assertEqual(rc, 1, out)
        self.assertIn("not signed by", out)

    def test_swapped_zip_fails_checksum(self):
        # the signature is good for the announced sha256, but the bytes served are different ones
        r = self.release("0.1.9-alpha")
        zp = os.path.join(self.srv, "Legend-of-Kairi-0.1.9-alpha.zip")
        make_zip(zp, extra=["Legend of Kairi 0.1.9-alpha/x.txt"])
        rc, out = self.run_su([r], "--yes", "--no-launch")
        self.assertEqual(rc, 1, out)
        # ... and announcing the swapped file's own size + sha256 breaks the signature instead
        data = open(zp, "rb").read()
        r["assets"][0].update(size=len(data), digest="sha256:" + hashlib.sha256(data).hexdigest())
        rc, out = self.run_su([r], "--yes", "--no-launch")
        self.assertEqual(rc, 1, out)
        self.assertIn("not signed by", out)

    def test_sigcheck_basics(self):
        m = sigcheck.message("a.zip", 5, "AB" * 32)
        s = sigcheck.sign(PRIV, m)
        self.assertTrue(sigcheck.verify(bytes.fromhex(PUB), s, m))
        self.assertFalse(sigcheck.verify(bytes.fromhex(PUB), s, m + b"x"))
        self.assertFalse(sigcheck.verify(bytes.fromhex(PUB), s[:-1] + bytes([s[-1] ^ 1]), m))
        self.assertFalse(sigcheck.verify(bytes.fromhex(PUB), s[:63], m))
        self.assertFalse(sigcheck.verify(b"", s, m))
        # RFC 8032 test vector 1 (empty message)
        pub = bytes.fromhex("d75a980182b10ab7d54bfed3c964073a0ee172f3daa62325af021a68f707511a")
        sig = bytes.fromhex("e5564300c360ac729086e2cc806e828a84877f1eb8e5d974d873e065224901555fb8821590a33bacc61e39701cf9b46bd25bf5f0595bbe24655141438e7a100b")
        self.assertTrue(sigcheck.verify(pub, sig, b""))
        self.assertEqual(sigcheck.public_of(bytes.fromhex("9d61b19deffd5a60ba844af492ec2cc44449c5697b326919703bac031cae7f60")), pub)

    def test_missing_digest(self):
        rc, out = self.run_su([self.release("0.1.9-alpha", digest=None)], "--yes", "--no-launch")
        self.assertEqual(rc, 1, out)
        self.assertFalse(os.path.exists(self.dl()))

    def test_zip_slip(self):
        for bad in ("Legend of Kairi 0.1.9-alpha/../evil.txt", "/abs.txt", "C:/evil.txt", "other/x.txt"):
            zp = os.path.join(self.srv, "Legend-of-Kairi-0.1.9-alpha.zip")
            make_zip(zp, extra=[bad])
            rc, out = self.run_su([self.release("0.1.9-alpha", zp=zp)], "--yes", "--no-launch")
            self.assertEqual(rc, 1, (bad, out))
            self.assertFalse(os.path.exists(os.path.join(TMP, "evil.txt")))
            self.assertFalse(os.path.exists(os.path.join(self.dl(), "Legend-of-Kairi-0.1.9-alpha")), bad)

    def test_draft_prerelease_skipped(self):
        rels = [self.release("0.2.0-alpha", draft=True), self.release("0.2.1-alpha", pre=True)]
        rc, out = self.run_su(rels + [self.release("0.1.9-alpha")], "--yes", "--no-launch")
        self.assertEqual(rc, 0, out)
        self.assertIn("0.1.9-alpha", out)
        self.assertNotIn("0.2.", out)

    def test_leftover_cleanup(self):
        junk = os.path.join(self.dl(), "old", "x")
        os.makedirs(junk)
        pathlib.Path(self.dl(), "junk.part").write_text("z")
        rc, out = self.run_su([self.release("0.1.9-alpha")], "--yes", "--no-launch")
        self.assertEqual(rc, 0, out)
        self.assertFalse(os.path.exists(os.path.join(self.dl(), "old")))
        self.assertFalse(os.path.exists(os.path.join(self.dl(), "junk.part")))

    def test_live_installer_blocks(self):
        d = os.path.join(self.dl(), "x", "Legend of Kairi 0.1.9-alpha", ".installer-ui")
        os.makedirs(d)
        # a live pid of a program OUTSIDE the download folder (a reused pid) does not block
        pathlib.Path(d, "lock").write_text(str(os.getpid()))
        rc, out = self.run_su([self.release("0.1.9-alpha")], "--yes", "--no-launch")
        self.assertEqual(rc, 0, out)
        # a program running from inside it (the new installer) blocks
        os.makedirs(d)
        exe = os.path.join(os.path.dirname(d), "kit", "pythonw.exe")
        os.makedirs(os.path.dirname(exe))
        shutil.copy2(os.path.join(os.environ.get("SystemRoot", "C:/Windows"), "System32", "PING.EXE"), exe)
        p = subprocess.Popen([exe, "-n", "30", "127.0.0.1"], stdout=subprocess.DEVNULL)
        try:
            pathlib.Path(d, "lock").write_text(str(p.pid))
            rc, out = self.run_su([self.release("0.1.9-alpha")], "--yes", "--no-launch")
            self.assertEqual(rc, 2, out)
            self.assertIn("still open", out)
            self.assertTrue(os.path.exists(d))
        finally:
            p.kill()
            p.wait()

    def test_bad_tag_refused(self):
        for tag in ("v0.1.9-alpha/../x", "v1.2.3\evil", "v9.9.9-a b", "latest", "v1.2.3.4.5", "v1.2.3-"):
            r = self.release("0.1.9-alpha")
            r["tag_name"] = tag
            r["assets"][0]["name"] = "Legend-of-Kairi-%s.zip" % (tag[1:] if tag[:1] == "v" else tag)
            rc, out = self.run_su([r], "--yes", "--no-launch")
            self.assertEqual(rc, 1, (tag, out))
            self.assertIn("Could not find any downloadable release", out)
            self.assertFalse(os.path.exists(self.dl()), tag)

    def test_wrong_host_refused_in_normal_mode(self):
        sys.path.insert(0, HERE)
        import argparse
        import selfupdate
        old = (selfupdate.DEV, selfupdate.find_newest)
        try:
            selfupdate.DEV = False
            for url in ("https://evil.example/releases/download/v9/Legend-of-Kairi-9.0.0.zip", "http://github.com/tvllerr/legend-of-kairi/releases/download/v9/x.zip",
                        "https://github.com/someoneelse/legend-of-kairi/releases/download/v9/x.zip", pathlib.Path(self.srv, "x.zip").as_uri()):
                asset = {"name": "Legend-of-Kairi-9.0.0.zip", "size": 10, "browser_download_url": url, "digest": "sha256:" + "0" * 64}
                selfupdate.find_newest = lambda api, asset=asset: ("v9.0.0", "9.0.0", asset, dict(asset, name=asset["name"] + ".sig"))
                a = argparse.Namespace(root=self.root, api=selfupdate.API, check=False, yes=True, force=False, no_launch=True, keys=selfupdate.PUBKEYS)
                with self.assertRaises(selfupdate.Fail) as cm:
                    selfupdate.run(a)
                self.assertIn("not one of this project", str(cm.exception), url)
            self.assertFalse(os.path.exists(os.path.join(self.dl(), "Legend-of-Kairi-9.0.0.zip")))
            # plain http / file:// are refused by the opener itself in normal mode
            with self.assertRaises(selfupdate.Fail):
                selfupdate.http_open("http://github.com/x")
            with self.assertRaises(selfupdate.Fail):
                selfupdate.http_open(pathlib.Path(self.srv, "x.zip").as_uri())
        finally:
            selfupdate.DEV, selfupdate.find_newest = old

    def test_redirect_must_stay_https(self):
        sys.path.insert(0, HERE)
        import selfupdate
        import urllib.request
        old = selfupdate.DEV
        try:
            selfupdate.DEV = False
            h = selfupdate._HttpsOnly()
            req = urllib.request.Request("https://github.com/x")
            with self.assertRaises(selfupdate.Fail):
                h.redirect_request(req, None, 302, "Found", {}, "http://objects.example/x.zip")
            ok = h.redirect_request(req, None, 302, "Found", {}, "https://release-assets.githubusercontent.com/x.zip")
            self.assertEqual(ok.full_url, "https://release-assets.githubusercontent.com/x.zip")
        finally:
            selfupdate.DEV = old

    def test_oversize_stream_aborted_early(self):
        sys.path.insert(0, HERE)
        import selfupdate

        class Endless:
            headers = {}
            def __init__(self):
                self.sent = 0
            def read(self, n):
                self.sent += n
                if self.sent > 200 << 20:
                    raise AssertionError("kept reading an endless stream")
                return b"x" * n
            def __enter__(self):
                return self
            def __exit__(self, *a):
                return False

        s = Endless()
        old = selfupdate.http_open
        selfupdate.http_open = lambda url, accept=None: s
        try:
            dest = os.path.join(TMP, "dl", "a.zip")
            with self.assertRaises(selfupdate.Fail) as cm:
                selfupdate.download("https://x/y", dest, 3 << 20, "0" * 64)
            self.assertIn("bigger than GitHub announced", str(cm.exception))
            self.assertLessEqual(s.sent, 5 << 20)
            self.assertFalse(os.path.exists(dest + ".part"))
        finally:
            selfupdate.http_open = old

    def test_api_json_capped(self):
        sys.path.insert(0, HERE)
        import selfupdate
        big = os.path.join(self.srv, "big.json")
        with open(big, "wb") as f:
            f.write(b"[" + b" " * (6 << 20) + b"]")
        old = selfupdate.DEV
        selfupdate.DEV = True
        try:
            with self.assertRaises(selfupdate.Fail) as cm:
                selfupdate.find_newest(pathlib.Path(big).as_uri())
            self.assertIn("bigger answer than expected", str(cm.exception))
        finally:
            selfupdate.DEV = old


if __name__ == "__main__":
    unittest.main(verbosity=1)
