import struct
def parse(d):
    bom=d[6:8]; e='<' if bom==b'\xff\xfe' else '>'
    hl,=struct.unpack(e+'H',d[4:6]); fsz,dataoff=struct.unpack(e+'II',d[8:16])
    assert d[hl:hl+4]==b'SFAT'
    n,=struct.unpack(e+'H',d[hl+6:hl+8])
    sfat=hl+12; sfnt=sfat+16*n; nameoff=sfnt+8
    out=[]
    for i in range(n):
        h,a,s,en=struct.unpack(e+'IIII',d[sfat+16*i:sfat+16*i+16])
        no=(a&0xffffff)*4
        nm=d[nameoff+no:d.index(b'\0',nameoff+no)].decode()
        out.append((nm,dataoff+s,en-s))
    return out,e,dataoff
if __name__=='__main__':
    import sys
    d=open(sys.argv[1],'rb').read()
    for nm,o,s in parse(d)[0]: print(nm,o,s)
