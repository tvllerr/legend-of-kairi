"""Kingdom Key (KH2 W_EX010 bind pose, from `sorabake kbexport`) orthographic render + cel-shade, assembled into the logo's sword
slot. numpy + Pillow only (port of tools_s110/logo kbrender.py + kb_layer.py). Built at INSTALL time from the player's own KH2 files:
nothing of it ships. Returns (rgb 0..255 float64 HxWx3, alpha float64 HxW)."""
import os
import numpy as np
from PIL import Image
import ndi_lite as ndi

_HI = {}


def _hi(kbdir, V, T, F, tex, ctr, s):
    if kbdir not in _HI:
        _HI[kbdir] = render(V - ctr, T, F, tex, rot('x', 90), 520, int(134 * s) + 40, s, 0, 0)
    return _HI[kbdir]


def load(d):
    V=[];T=[];F=[]
    cur=None
    for l in open(d+'/mesh.obj'):
        p=l.split()
        if p[0]=='v': V.append(list(map(float,p[1:4])))
        elif p[0]=='vt': T.append(list(map(float,p[1:3])))
        elif p[0]=='f': F.append([int(x.split('/')[0])-1 for x in p[1:4]])
    tex=np.frombuffer(open(d+'/t0_256x128.rgba','rb').read(),np.uint8).reshape(128,256,4)
    return np.array(V),np.array(T),np.array(F),tex
def render(V,T,F,tex,R,W,H,scale,cx,cy,cull=None,flat=False):
    """V rotated by R (3x3); screen x=+X, y=-Y (up), depth = Z (larger = nearer). returns RGBA float array"""
    P=V@R.T
    sx=(P[:,0]-cx)*scale+W/2; sy=-(P[:,1]-cy)*scale+H/2; z=P[:,2]
    col=np.zeros((H,W,4),np.float32); zb=np.full((H,W),-1e9,np.float32)
    for f in F:
        a,b,c=f
        x0,y0,x1,y1,x2,y2=sx[a],sy[a],sx[b],sy[b],sx[c],sy[c]
        den=(y1-y2)*(x0-x2)+(x2-x1)*(y0-y2)
        if abs(den)<1e-9: continue
        # backface: normal z
        n=np.cross(P[b]-P[a],P[c]-P[a])[2]
        if cull=='back' and n<0: continue
        if cull=='front' and n>0: continue
        minx=max(int(np.floor(min(x0,x1,x2))),0); maxx=min(int(np.ceil(max(x0,x1,x2))),W-1)
        miny=max(int(np.floor(min(y0,y1,y2))),0); maxy=min(int(np.ceil(max(y0,y1,y2))),H-1)
        if minx>maxx or miny>maxy: continue
        xs,ys=np.meshgrid(np.arange(minx,maxx+1)+.5,np.arange(miny,maxy+1)+.5)
        l0=((y1-y2)*(xs-x2)+(x2-x1)*(ys-y2))/den
        l1=((y2-y0)*(xs-x2)+(x0-x2)*(ys-y2))/den
        l2=1-l0-l1
        m=(l0>=-1e-4)&(l1>=-1e-4)&(l2>=-1e-4)
        if not m.any(): continue
        zz=l0*z[a]+l1*z[b]+l2*z[c]
        u=l0*T[a,0]+l1*T[b,0]+l2*T[c,0]; v=l0*T[a,1]+l1*T[b,1]+l2*T[c,1]
        ui=np.clip(((u%1)*tex.shape[1]).astype(int),0,tex.shape[1]-1); vi=np.clip(((v%1)*tex.shape[0]).astype(int),0,tex.shape[0]-1)
        c4=tex[vi,ui].astype(np.float32)
        if flat:
            shade=0.55+0.45*abs(n)/(np.linalg.norm(np.cross(P[b]-P[a],P[c]-P[a]))+1e-9)
            c4[...,:3]*=shade
        sub=zb[miny:maxy+1,minx:maxx+1]
        upd=m&(zz>sub)&(c4[...,3]>20)
        sub[upd]=zz[upd]
        col[miny:maxy+1,minx:maxx+1][upd]=c4[upd]
    return col
def rot(ax,deg):
    t=np.radians(deg);c,s=np.cos(t),np.sin(t)
    if ax=='x': return np.array([[1,0,0],[0,c,-s],[0,s,c]])
    if ax=='y': return np.array([[c,0,s],[0,1,0],[-s,0,c]])
    return np.array([[c,-s,0],[s,c,0],[0,0,1]])

"""Kingdom Key (W_EX010 bind pose) orthographic render, assembled to the Master Sword's slot in the logo (412x306 RGBA)."""
def build(kbdir,S=2.1,XS=1.35,top=4,tip=295,cx=79.5,W=412,H=306,cache=None):
    import os
    V,T,F,tex=load(kbdir)
    ctr=(V.min(0)+V.max(0))/2
    s=8.8
    col=_hi(kbdir,V,T,F,tex,ctr,s)
    f=S/s                                  # hi px -> final px
    # segments (source rows in hi-res): charm | [cut chain links] | ring+clasp+guard | plain shaft (stretched) | teeth
    segs=[(19,100,'keep'),(300,600,'keep'),(600,1040,'shaft'),(1040,1200,'keep')]
    keep_h=sum((b-a)*f for a,b,k in segs if k!='shaft')
    shaft_src=(1040-600)*f
    shaft_dst=(tip-top)-keep_h
    g=shaft_dst/shaft_src
    # build output at 4x final res rows
    SS=4
    hh=int((tip-top+4)*SS)
    out_rows=[]; y=0.0
    pieces=[]
    for a,b,k in segs:
        sc=f*(g if k=='shaft' else 1.0)*SS      # hi px -> SS px (row scale)
        n=int(round((b-a)*sc))
        pieces.append((a,b,n))
    tot=sum(n for _,_,n in pieces)
    img=np.zeros((tot,col.shape[1],4),np.float32)
    y0=0
    for a,b,n in pieces:
        seg=col[a:b].copy()
        # resize rows to n via linear interp in premultiplied space
        pm=seg.copy(); pm[...,:3]*=pm[...,3:4]/255.
        idx=np.linspace(0,b-a-1,n)
        i0=np.floor(idx).astype(int); i1=np.minimum(i0+1,b-a-1); fr=(idx-i0)[:,None,None]
        r=pm[i0]*(1-fr)+pm[i1]*fr
        img[y0:y0+n]=r; y0+=n
    # un-premultiply later; resize horizontally: hi px -> SS px * f * XS
    nw=int(round(col.shape[1]*f*SS*XS))
    pm=Image.fromarray(img.clip(0,255).astype(np.uint8),'RGBA')   # premultiplied rgb in 8 bit
    pm=pm.resize((nw,tot),Image.LANCZOS)
    arr=np.array(pm).astype(np.float32)
    # place on SS canvas
    CW,CH=W*SS,H*SS
    can=np.zeros((CH,CW,4),np.float32)
    # shaft centre column in hi = 259.5
    cxs=int(round(cx*SS-259.5*f*SS*XS)); cys=int(round(top*SS))
    ya,yb=max(cys,0),min(cys+tot,CH); xa,xb=max(cxs,0),min(cxs+nw,CW)
    can[ya:yb,xa:xb]=arr[ya-cys:yb-cys,xa-cxs:xb-cxs]
    a=can[...,3]
    rgb=np.where(a[...,None]>0,can[...,:3]/np.maximum(a[...,None],1)*255.,0).clip(0,255)
    # downsample
    A=Image.fromarray(a.clip(0,255).astype(np.uint8)).resize((W,H),Image.BOX)
    P=Image.fromarray((can[...,:3]).clip(0,255).astype(np.uint8)).resize((W,H),Image.BOX)   # premult colour (0..255 scaled by a/255)
    Aa=np.array(A).astype(np.float32); Pp=np.array(P).astype(np.float32)
    Cc=np.where(Aa[...,None]>0,Pp/np.maximum(Aa[...,None]/255.,1e-3),0).clip(0,255)
    # outline: dark, outside, 1.3 px
    m=Aa/255.
    d=ndi.edt_small(m>=0.5)
    ol=np.clip(1.4-d,0,1)*(m<0.99)
    oa=np.clip(ol*0.85+0,0,1)
    outc=np.array([34,26,44],np.float32)
    # composite sword over outline
    fa=m+(1-m)*oa
    fc=(Cc*m[...,None]+outc*((1-m)*oa)[...,None])/np.maximum(fa[...,None],1e-3)
    return paint(fc,fa)

def paint(fc,fa,seed=11):
    """cel-shade + wear so the blade reads as part of the painted logo (cf. the Master Sword: flat tone bands, a pale sliver on the
    left, dark right side, dark cracks, chipped silhouette)"""
    import colorsys
    H,W=fa.shape; rng=np.random.default_rng(seed)
    a=fa.copy(); rgb=fc.copy()/255.
    H,W=fa.shape; rng=np.random.default_rng(seed)
    a=fa.copy(); rgb=fc.copy()/255.
    mx=rgb.max(2); mn=rgb.min(2); v=mx
    sat=np.where(mx>1e-3,(mx-mn)/np.maximum(mx,1e-3),0)
    levels=np.array([0.10,0.26,0.42,0.58,0.74,0.90])
    vi=np.abs(v[...,None]-levels).argmin(2)
    ramp=np.array([(30,38,52),(62,86,104),(100,135,150),(140,175,186),(196,218,224),(240,248,248)],np.float32)/255.
    vq=levels[vi]
    gold=rgb*(vq/np.maximum(v,1e-3))[...,None]                       # posterised, hue kept (gold guard, black grip, blue band)
    steel=ramp[vi]
    isst=(sat<0.22)&(v>0.30)                                           # grey metal -> the sword's cold steel tones
    rgb=np.where(isst[...,None],steel,gold)
    solid=a>0.5
    # rust / wear patches (the sword's brown), mostly on the plain shaft
    def nz(scale):
        g=rng.standard_normal((H//scale+3,W//scale+3)); z=ndi.zoom1(g,scale)[:H,:W]; return z/z.std()
    n1=nz(7)+0.6*nz(3)
    rust=(n1>1.15)&isst&ndi.erode4(solid,iterations=1)
    rgb[rust]=np.array([(104,62,44),(140,86,60),(84,48,36)],np.float32)[rng.integers(0,3,rust.sum())]/255.
    # chip the silhouette
    edge=solid&~ndi.erode4(solid,iterations=1)
    a=np.where((rng.random((H,W))<0.22)&edge,0,a)
    # light left sliver / dark right side per run
    for y in range(H):
        xs=np.where(a[y]>0.5)[0]
        if len(xs)==0: continue
        for r in np.split(xs,np.where(np.diff(xs)>1)[0]+1):
            if len(r)<8: continue
            l,rr=r[0],r[-1]
            rgb[y,l+1:l+3]=np.clip(rgb[y,l+1:l+3]*0.3+0.7,0,1)
            rgb[y,max(rr-2,l):rr]*=0.5
    # dark cracks
    ys,xs=np.where(ndi.erode4(solid,iterations=3))
    if len(ys):
        for _ in range(18):
            k=rng.integers(len(ys)); y0,x0=ys[k],xs[k]; ln=int(rng.integers(5,12)); sl=rng.choice([-0.7,-0.45,0.5])
            for t in range(ln):
                yy=int(y0+t); xx=int(x0+sl*t)
                if 0<=yy<H and 0<=xx<W and solid[yy,xx]: rgb[yy,xx]=np.minimum(rgb[yy,xx],np.array([0.14,0.09,0.09]))
    # darker outline stays from build(); re-darken pixels near the rim
    return (rgb*255).clip(0,255),a

