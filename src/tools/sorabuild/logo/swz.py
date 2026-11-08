import numpy as np
def _gob_offset_table():
    # offset within a 512-byte GOB (64 bytes wide x 8 rows) for byte x (0..63), row y (0..7)
    t=np.zeros((8,64),dtype=np.int64)
    for y in range(8):
        for x in range(64):
            t[y,x]=((x%64)//32)*256+((y%8)//2)*64+((x%32)//16)*32+(y%2)*16+(x%16)
    return t
GOB=_gob_offset_table()
def layout(wb,hb,bpb,blkh_log2):
    """wb,hb: size in blocks; bpb bytes per block. returns (index array of src offsets, total size)"""
    bh=1<<blkh_log2
    wbytes=wb*bpb
    gobw=(wbytes+63)//64
    gobh=(hb+7)//8
    gobblocks=(gobh+bh-1)//bh
    size=gobw*gobblocks*bh*512
    ys=np.arange(hb)[:,None]; xs=(np.arange(wb)[None,:,None]*bpb+np.arange(bpb)[None,None,:]).reshape(1,-1)
    gy=ys//8; gx=xs//64
    blk=gy//bh; gin=gy%bh
    off=(blk*gobw+gx)*bh*512 + gin*512 + GOB[ys%8, xs%64]
    return off, size
def deswizzle(data,wb,hb,bpb,blkh_log2):
    off,size=layout(wb,hb,bpb,blkh_log2)
    a=np.frombuffer(data,dtype=np.uint8)
    return a[off].reshape(hb,wb*bpb)
def swizzle(lin,wb,hb,bpb,blkh_log2):
    off,size=layout(wb,hb,bpb,blkh_log2)
    out=np.zeros(size,dtype=np.uint8)
    out[off]=lin.reshape(hb,wb*bpb)
    return out.tobytes()
