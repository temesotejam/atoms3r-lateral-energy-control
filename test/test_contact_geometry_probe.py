import math,random,sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from contact_geometry_probe import min_height, switch, R,INNER,OUTER,WIDTH

def brute(side,sway,pitch,leg,H):
    s,p,q=map(math.radians,(sway,pitch,leg));a=math.sqrt(R*R-INNER*INNER)
    return min(-math.sin(s)*x+math.cos(s)*(H*math.cos(p)+math.sin(p+q)*y+
               math.cos(p+q)*(a-math.sqrt(R*R-x*x)-H))
               for x in (side*(INNER+(OUTER-INNER)*i/3000) for i in range(3001))
               for y in (-WIDTH/2,WIDTH/2))

random.seed(4)
for _ in range(40):
    s,p,q=[random.uniform(-8,8) for i in range(3)]
    for side in (-1,1):assert abs(min_height(side,s,p,q)-brute(side,s,p,q,120))<1e-5
assert abs(switch(0,0,0))<1e-8
assert abs(switch(2,0,2)+switch(2,2,0))<1e-8
print('PASS: curved-foot analytical minima agree with sampled surface; symmetry verified')
