import struct, sys
TYPES = {0:"dummy",1:"binary",2:"list",3:"bdx",4:"mdlx",5:"mset?",6:"tm2",7:"imgd",8:"seqd",9:"imgz",10:"?",11:"layd",12:"pax?",17:"apdx",18:"limi",19:"?",20:"?",24:"seqd2"}
def entries(b):
    n = struct.unpack_from("<I", b, 4)[0]
    r = []
    for i in range(n):
        o = 0x10 + i*16
        t = struct.unpack_from("<I", b, o)[0]
        nm = b[o+4:o+8].split(b"\0")[0].decode("latin1")
        off, sz = struct.unpack_from("<II", b, o+8)
        r.append((i, t, nm, off, sz))
    return r
if __name__ == "__main__":
    b = open(sys.argv[1], "rb").read()
    for e in entries(b): print(e)
