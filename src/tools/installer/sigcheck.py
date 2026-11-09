"""Ed25519 signatures for Legend of Kairi updates (s186). No own crypto: calls the OpenSSL 3 libcrypto that ships with Python
(kit/python/libcrypto-3.dll) through ctypes. selfupdate.py only verifies; signrel.py (dev only, never shipped) signs.

What is signed is not the zip itself but this short text, so the signature can be checked BEFORE the big download and the
download is then checked against the signed sha256:
    legend-of-kairi-update-v1\n<asset name>\n<size in bytes>\n<sha256 hex, lower case>\n
The asset name carries the version, so an old signed release can never be passed off as a newer one."""
import ctypes, os, sys

ED25519 = 1087          # NID_ED25519 / EVP_PKEY_ED25519
_lib = None


class SigError(Exception):
    pass


def message(name, size, sha256_hex):
    return ("legend-of-kairi-update-v1\n%s\n%d\n%s\n" % (name, int(size), sha256_hex.lower())).encode("utf-8")


def _crypto():
    global _lib
    if _lib is not None:
        return _lib
    base = os.path.dirname(os.path.abspath(sys.executable))
    # full paths only, next to the running python (never the DLL search path)
    cands = [os.path.join(base, n) for n in ("libcrypto-3.dll", "libcrypto-3-x64.dll", os.path.join("DLLs", "libcrypto-3.dll"),
                                              os.path.join("DLLs", "libcrypto-3-x64.dll"))]
    for p in cands:
        if os.path.isfile(p):
            try:
                L = ctypes.CDLL(p)
            except OSError:
                continue
            vp, sz, ci, cp = ctypes.c_void_p, ctypes.c_size_t, ctypes.c_int, ctypes.c_char_p
            try:
                for name, res, args in (
                        ("EVP_PKEY_new_raw_public_key", vp, [ci, vp, cp, sz]),
                        ("EVP_PKEY_new_raw_private_key", vp, [ci, vp, cp, sz]),
                        ("EVP_PKEY_get_raw_public_key", ci, [vp, cp, ctypes.POINTER(sz)]),
                        ("EVP_PKEY_free", None, [vp]),
                        ("EVP_MD_CTX_new", vp, []),
                        ("EVP_MD_CTX_free", None, [vp]),
                        ("EVP_DigestVerifyInit", ci, [vp, vp, vp, vp, vp]),
                        ("EVP_DigestVerify", ci, [vp, cp, sz, cp, sz]),
                        ("EVP_DigestSignInit", ci, [vp, vp, vp, vp, vp]),
                        ("EVP_DigestSign", ci, [vp, cp, ctypes.POINTER(sz), cp, sz])):
                    f = getattr(L, name)
                    f.restype, f.argtypes = res, args
            except AttributeError:
                continue
            _lib = L
            return L
    raise SigError("OpenSSL (libcrypto-3.dll) was not found next to python")


def verify(pub, sig, msg):
    """True only when sig (64 bytes) is a valid Ed25519 signature of msg under pub (32 bytes). Bad sizes -> False."""
    if not (isinstance(pub, bytes) and isinstance(sig, bytes) and isinstance(msg, bytes)) or len(pub) != 32 or len(sig) != 64:
        return False
    L = _crypto()
    key = L.EVP_PKEY_new_raw_public_key(ED25519, None, pub, 32)
    if not key:
        return False
    ctx = L.EVP_MD_CTX_new()
    try:
        if not ctx or L.EVP_DigestVerifyInit(ctx, None, None, None, key) != 1:
            return False
        return L.EVP_DigestVerify(ctx, sig, 64, msg, len(msg)) == 1
    finally:
        if ctx:
            L.EVP_MD_CTX_free(ctx)
        L.EVP_PKEY_free(key)


def public_of(priv):
    if not isinstance(priv, bytes) or len(priv) != 32:
        raise SigError("a private key is 32 bytes")
    L = _crypto()
    key = L.EVP_PKEY_new_raw_private_key(ED25519, None, priv, 32)
    if not key:
        raise SigError("OpenSSL refused the private key")
    try:
        buf = ctypes.create_string_buffer(32)
        n = ctypes.c_size_t(32)
        if L.EVP_PKEY_get_raw_public_key(key, buf, ctypes.byref(n)) != 1 or n.value != 32:
            raise SigError("could not derive the public key")
        return buf.raw
    finally:
        L.EVP_PKEY_free(key)


def sign(priv, msg):
    if not isinstance(priv, bytes) or len(priv) != 32:
        raise SigError("a private key is 32 bytes")
    L = _crypto()
    key = L.EVP_PKEY_new_raw_private_key(ED25519, None, priv, 32)
    if not key:
        raise SigError("OpenSSL refused the private key")
    ctx = L.EVP_MD_CTX_new()
    try:
        buf = ctypes.create_string_buffer(64)
        n = ctypes.c_size_t(64)
        if not ctx or L.EVP_DigestSignInit(ctx, None, None, None, key) != 1 or \
                L.EVP_DigestSign(ctx, buf, ctypes.byref(n), msg, len(msg)) != 1 or n.value != 64:
            raise SigError("signing failed")
        return buf.raw
    finally:
        if ctx:
            L.EVP_MD_CTX_free(ctx)
        L.EVP_PKEY_free(key)
