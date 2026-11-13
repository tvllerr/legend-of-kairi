"""pesig (s201): check a native file (.exe / .dll) against a plain sha256 pin, also when somebody added an Authenticode signature.

Why: Melty signs the native files of a mod at install (Smart App Control). Signing a PE appends a certificate table, pads the file
to 8 bytes before it with zeros, writes the table's place into the Security data directory (index 4) and recomputes the optional
header CheckSum. Nothing else changes (checked on the kit's vgmstream exe + 11 DLLs, s201). So the original bytes can be rebuilt
from the signed file: cut the table (and the zero padding), zero the Security entry and put back a CheckSum. The original
CheckSum is not in the signed file, so the candidates are: the signed file's own (a signer that leaves it), 0 (what our MinGW
builds have) and the PE checksum of the rebuilt bytes (what a linker writes). A pin matches when the sha256 of one rebuild IS
the pinned sha256: the same proof as a plain sha256 check (a different file cannot pass), the pins and manifests stay as they are.

  matches(path, sha256_hex, size=None) -> "unsigned" (plain sha256 + size match), "signed" (a rebuild matches) or None
  stripped_sha256(path)                -> sha256 with the table + trailing zero bytes cut and CheckSum + Security zeroed (PE),
                                          the plain sha256 for anything else: the same for a file and its signed copy (logs, tests)
  signed(path)                         -> True if the file is a PE with a non-zero Security directory
Pure stdlib (the kit's python runs with -E -P / -I). A file that is not a PE, or a broken / truncated one, gets the plain sha256.
"""
import hashlib
import struct

SEC_INDEX = 4   # IMAGE_DIRECTORY_ENTRY_SECURITY


def _read(path):
    with open(path, "rb") as f:
        return f.read()


def layout(d):
    """-> (CheckSum offset, Security entry offset, cert table offset, cert table size) of a PE, or None (not a PE / broken).
    The table offset/size are 0, 0 when the file is not signed."""
    try:
        if len(d) < 0x40 or d[:2] != b"MZ":
            return None
        pe = struct.unpack_from("<I", d, 0x3C)[0]
        if pe < 0x40 or pe + 24 > len(d) or d[pe:pe + 4] != b"PE\0\0":
            return None
        opt_size = struct.unpack_from("<H", d, pe + 20)[0]
        opt = pe + 24
        magic = struct.unpack_from("<H", d, opt)[0]
        if magic == 0x10B:
            nrva_off, dd = opt + 92, opt + 96
        elif magic == 0x20B:
            nrva_off, dd = opt + 108, opt + 112
        else:
            return None
        sec = dd + 8 * SEC_INDEX
        if sec + 8 > opt + opt_size or sec + 8 > len(d) or struct.unpack_from("<I", d, nrva_off)[0] <= SEC_INDEX:
            return None
        off, size = struct.unpack_from("<II", d, sec)
        if (off or size) and (off < sec + 8 or size < 8 or off + size != len(d)):
            return None   # a Security entry that does not point at a table at the end of the file: not one we can cut
        return opt + 64, sec, off, size
    except struct.error:
        return None


def pe_checksum(d, ck_off):
    """the PE optional header CheckSum of d (the 4 bytes at ck_off count as 0): 16-bit ones' complement sum + file length"""
    b = bytearray(d)
    b[ck_off:ck_off + 4] = b"\0\0\0\0"
    # the folded 16-bit word sum == (the file as one little-endian number) mod 0xFFFF, as 2^16 == 1 (mod 0xFFFF);
    # folding never gives 0 once a word was non-zero, so 0 -> 0xFFFF then
    n = int.from_bytes(b, "little")
    s = n % 0xFFFF
    if s == 0 and n:
        s = 0xFFFF
    return (s + len(d)) & 0xFFFFFFFF


def signed(path):
    lay = layout(_read(path))
    return bool(lay and lay[2])


def _unsigned_body(d, lay):
    """the signed file without its table, Security entry zeroed (bytearray; the zero padding is still on)"""
    ck_off, sec, off, _ = lay
    b = bytearray(d[:off])
    b[sec:sec + 8] = b"\0" * 8
    return b


def stripped_sha256(path):
    d = _read(path)
    lay = layout(d)
    if not lay:
        return hashlib.sha256(d).hexdigest()
    b = _unsigned_body(d, lay) if lay[2] else bytearray(d)
    b[lay[0]:lay[0] + 4] = b"\0\0\0\0"
    return hashlib.sha256(bytes(b).rstrip(b"\0")).hexdigest()


def matches(path, sha256_hex, size=None):
    """"unsigned" | "signed" | None: is this file the pinned one (sha256 + size when given), maybe with a signature added?"""
    want = sha256_hex.lower()
    d = _read(path)
    if (size is None or len(d) == size) and hashlib.sha256(d).hexdigest() == want:
        return "unsigned"
    lay = layout(d)
    if not lay or not lay[2]:
        return None
    ck_off, _, off, _ = lay
    body = _unsigned_body(d, lay)
    # the original length: the table starts at it, or up to 7 zero bytes of padding later
    lens = [n for n in range(off, max(off - 8, ck_off + 4), -1) if not any(body[n:off])]
    if size is not None:
        lens = [n for n in lens if n == size]
    signed_ck = struct.unpack_from("<I", d, ck_off)[0]
    for n in lens:
        b = body[:n]
        for ck in dict.fromkeys((signed_ck, 0, pe_checksum(b, ck_off))):
            struct.pack_into("<I", b, ck_off, ck)
            if hashlib.sha256(b).hexdigest() == want:
                return "signed"
    return None
