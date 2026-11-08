import struct
def decompress(d):
    assert d[:4]==b'Yaz0'
    n,=struct.unpack('>I',d[4:8]); out=bytearray(n); i=16; o=0
    while o<n:
        c=d[i]; i+=1
        for b in range(8):
            if o>=n: break
            if c&(0x80>>b):
                out[o]=d[i]; o+=1; i+=1
            else:
                b1,b2=d[i],d[i+1]; i+=2
                dist=((b1&15)<<8|b2)+1; ln=b1>>4
                if ln==0: ln=d[i]+0x12; i+=1
                else: ln+=2
                for _ in range(ln):
                    out[o]=out[o-dist]; o+=1
    return bytes(out)
def compress(raw,level=1):
    # simple hash-chain LZ encoder
    n=len(raw); out=bytearray(b'Yaz0'+struct.pack('>I',n)+b'\0'*8)
    head={}; i=0
    grp=bytearray(); flags=0; cnt=0
    def flush():
        nonlocal grp,flags,cnt
        if cnt:
            out.append(flags<<(8-cnt) & 0xff if False else flags); out.extend(grp)
        grp=bytearray(); flags=0; cnt=0
    # use bits MSB first
    flags=0;cnt=0
    def emit(bit,bs):
        nonlocal flags,cnt,grp
        flags=(flags<<1)|bit; cnt+=1; grp+=bs
        if cnt==8:
            out.append(flags); out.extend(grp); flags=0;cnt=0;grp=bytearray()
    chains={}
    def ins(p):
        if p+3<=n:
            chains.setdefault(raw[p:p+3],[]).append(p)
    while i<n:
        best=0;bd=0
        if i+3<=n:
            lst=chains.get(raw[i:i+3])
            if lst:
                for p in reversed(lst[-16:]):
                    d=i-p
                    if d>4096: break
                    l=3; mx=min(0x111,n-i)
                    while l<mx and raw[p+l]==raw[i+l]: l+=1
                    if l>best: best=l;bd=d
                    if l==mx: break
        if best>=3:
            dd=bd-1
            if best<18: emit(0,bytes([((best-2)<<4)|(dd>>8),dd&255]))
            else: emit(0,bytes([dd>>8,dd&255,best-0x12]))
            for k in range(best): ins(i+k)
            i+=best
        else:
            emit(1,bytes([raw[i]])); ins(i); i+=1
    if cnt:
        flags<<=(8-cnt); out.append(flags); out.extend(grp)
    return bytes(out)
