# botw_spots.py <MainField dir> : for each Junior/Middle Lynel, y spread of actors within 40 m and the best of 8 spots 25 m away
import sys, os, math
exec(open(os.path.join(os.path.dirname(os.path.abspath(__file__)), 'botw_lynels.py')).read().split("def main():")[0])
root = sys.argv[1]
acts = []; lyn = []
for unit in sorted(os.listdir(root)):
    for kind in ('Static', 'Dynamic'):
        f = os.path.join(root, unit, f'{unit}_{kind}.smubin')
        if not os.path.exists(f): continue
        for o in Byml(yaz0(open(f, 'rb').read())).parse().get('Objs', []):
            t = o.get('Translate')
            if not t: continue
            n = o.get('UnitConfigName', '')
            acts.append((t[0], t[1], t[2], n))
            if 'Enemy_Lynel' in n: lyn.append((unit, n, t, o.get('Rotate')))
for unit, n, t, rot in lyn:
    near = [a for a in acts if (a[0]-t[0])**2 + (a[2]-t[2])**2 <= 40**2 and not a[3].startswith('Enemy_Lynel')]
    ys = [a[1] for a in near]
    best = None
    for k in range(8):
        ang = k * math.pi / 4
        sx, sz = t[0] + 25*math.cos(ang), t[2] + 25*math.sin(ang)
        s = [a[1] for a in acts if (a[0]-sx)**2 + (a[2]-sz)**2 <= 10**2]
        if len(s) >= 2:
            dev = max(abs(v - t[1]) for v in s)
            if best is None or dev < best[0]: best = (dev, sx, sz, sum(s)/len(s), len(s))
    print(unit, n, [round(v, 1) for v in t], 'near40:', len(ys), ('y %.1f..%.1f' % (min(ys), max(ys))) if ys else '-', 'best25:', best and tuple(round(v, 1) for v in best))
