# botw_near.py <MainField dir> x y z radius : actors within radius (xz) of a point, any unit (verifies coordinate frame)
import sys, os
sys.argv_save = sys.argv
exec(open(os.path.join(os.path.dirname(os.path.abspath(__file__)), 'botw_lynels.py')).read().split("def main():")[0])
root = sys.argv[1]; x, y, z, r = map(float, sys.argv[2:6])
for unit in sorted(os.listdir(root)):
    for kind in ('Static', 'Dynamic'):
        f = os.path.join(root, unit, f'{unit}_{kind}.smubin')
        if not os.path.exists(f): continue
        for o in Byml(yaz0(open(f, 'rb').read())).parse().get('Objs', []):
            t = o.get('Translate')
            if t and ((t[0]-x)**2 + (t[2]-z)**2) ** .5 <= r:
                print(unit, kind, o.get('UnitConfigName'), [round(v, 1) for v in t])
