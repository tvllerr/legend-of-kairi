import struct,sys
def bar(path):
    d=open(path,'rb').read()
    assert d[:4]==b'BAR\x01',d[:4]
    n,=struct.unpack_from('<I',d,4)
    out=[]
    for i in range(n):
        t,dup,nm,off,sz=struct.unpack_from('<HH4sII',d,16+i*16)
        out.append((i,t,nm.split(b'\0')[0].decode('latin1'),off,sz))
    return d,out
if __name__=='__main__':
    d,e=bar(sys.argv[1])
    for x in e: print(*x)
