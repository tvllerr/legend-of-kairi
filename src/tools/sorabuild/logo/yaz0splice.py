"""Replace a region of a Yaz0 stream's decoded data while keeping every other byte of the stream identical.
Groups are decoded to find boundaries; only the groups covering [a-pad, b+4096) are re-encoded (greedy LZ, window = that region)."""
import struct
def group_index(d):
    n,=struct.unpack('>I',d[4:8]); i=16; o=0; g=[]
    while o<n:
        g.append((i,o)); c=d[i]; i+=1
        for b in range(8):
            if o>=n: break
            if c&(0x80>>b): o+=1; i+=1
            else:
                b1=d[i]; i+=2
                ln=b1>>4
                if ln==0: ln=d[i]+0x12; i+=1
                else: ln+=2
                o+=ln
    return g,i,n
DEPTH=400
def min_reach(d,g,n):
    """per group: smallest source offset (decoded coordinates) any back-reference of that group reads"""
    res=[]
    for k,(i,o) in enumerate(g):
        m=1<<60; c=d[i]; i+=1
        for b in range(8):
            if o>=n: break
            if c&(0x80>>b): o+=1; i+=1
            else:
                b1,b2=d[i],d[i+1]; i+=2
                dist=((b1&15)<<8|b2)+1; ln=b1>>4
                if ln==0: ln=d[i]+0x12; i+=1
                else: ln+=2
                m=min(m,o-dist); o+=ln
        res.append(m)
    return res
def encode_window(raw,start,end,tail_ok=True):
    """tokens covering raw[start:end] exactly, total items multiple of 8. returns bytes (groups)"""
    chains={}
    toks=[]   # (kind, bytes, endpos)
    i=start
    def ins(p):
        if p+3<=end: chains.setdefault(raw[p:p+3],[]).append(p)
    def find(i):
        best=0;bd=0
        if i+3<=end:
            lst=chains.get(raw[i:i+3])
            if lst:
                for p in reversed(lst[-DEPTH:]):
                    dd=i-p
                    if dd>4096: break
                    l=3; mx=min(0x111,end-i)
                    while l<mx and raw[p+l]==raw[i+l]: l+=1
                    if l>best: best=l;bd=dd
                    if l==mx: break
        return best,bd
    while i<end:
        best,bd=find(i)
        if best>=3 and best<0x111 and i+1<end:     # lazy: a clearly longer match one byte later wins
            ins(i); nb,nd=find(i+1)
            if nb>best+1:
                toks.append((1,bytes([raw[i]]),i+1)); i+=1; continue
            chains[raw[i:i+3]].pop()
        if best>=3:
            q=bd-1
            bs=bytes([((best-2)<<4)|(q>>8),q&255]) if best<18 else bytes([q>>8,q&255,best-0x12])
            toks.append((0,bs,i+best,bd,best))
            for k in range(best): ins(i+k)
            i+=best
        else:
            toks.append((1,bytes([raw[i]]),i+1)); ins(i); i+=1
    # make the item count a multiple of 8 by splitting back-references (same distance, shorter pieces) or, failing
    # that, turning the last few bytes into literals
    def mk(dist,ln):
        q=dist-1
        return bytes([((ln-2)<<4)|(q>>8),q&255]) if ln<18 else bytes([q>>8,q&255,ln-0x12])
    need=(-len(toks))%8
    for t in range(len(toks)-1,-1,-1):
        if need==0: break
        kind,bs,ep,*rest=toks[t]
        if kind==0:
            dist,ln=rest
            if ln>=6:
                toks[t:t+1]=[(0,mk(dist,3),ep-ln+3,dist,3),(0,mk(dist,ln-3),ep,dist,ln-3)]
                need-=1
    if need:
        raise Exception('cannot align')
    out=bytearray()
    for g in range(0,len(toks),8):
        fl=0; data=bytearray()
        for j,(kind,bs,*_) in enumerate(toks[g:g+8]):
            if kind: fl|=0x80>>j
            data+=bs
        out.append(fl); out+=data
    return bytes(out)
def splice(orig,newraw,a,b):
    g,endi,n=group_index(orig)
    assert len(newraw)==n
    gs=[x for x in g if x[1]<=a-8][-1]
    mr=min_reach(orig,g,n)
    suf=[1<<60]*(len(g)+1)
    for k in range(len(g)-1,-1,-1): suf[k]=min(suf[k+1],mr[k])
    ge=[g[k] for k in range(len(g)) if g[k][1]>=b and suf[k]>=b][0]
    enc=encode_window(newraw,gs[1],ge[1])
    return orig[:gs[0]]+enc+orig[ge[0]:], (gs,ge,len(enc),ge[0]-gs[0])
