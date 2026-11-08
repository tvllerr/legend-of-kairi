import struct
def textures(d):
    cnt,=struct.unpack_from('<I',d,0x24); ipo,=struct.unpack_from('<Q',d,0x28)
    out=[]
    for i in range(cnt):
        p,=struct.unpack_from('<Q',d,ipo+8*i)
        assert d[p:p+4]==b'BRTI'
        (flags,dim,tm,swz,mips,msaa,fmt,acc,w,h,depth,arr,blk,pad)=struct.unpack_from('<BBHHHIIIIIIIII',d,p+0x10)[:14]
        gx,=struct.unpack_from('<I',d,p+0x50) if False else (0,)
        imgsz,=struct.unpack_from('<I',d,p+0x50)
        align,=struct.unpack_from('<I',d,p+0x54)
        nmp,=struct.unpack_from('<Q',d,p+0x60); ln,=struct.unpack_from('<H',d,nmp)
        name=d[nmp+2:nmp+2+ln].decode()
        mipptr,=struct.unpack_from('<Q',d,p+0x70)
        offs=struct.unpack_from('<%dQ'%mips,d,mipptr)
        out.append(dict(i=i,p=p,name=name,fmt=fmt,w=w,h=h,mips=mips,blk=blk,imgsz=imgsz,align=align,offs=offs,tm=tm,arr=arr,pad=pad))
    return out
