import json,subprocess,sys,math
from pathlib import Path
d=json.loads(Path('tools/fixtures/walking_loss_50881356.json').read_text())
r=d['records'];assert len(r)==32
assert all(x['pulse_ms']==0 and not x['output'] for x in r)
assert all(5000<=x['zero_ms']<x['peak_ms']<30000 for x in r)
text=''.join(' '.join(str(x[k]) for k in ('rate_dps','rpm','vbat_mv','previous_deg','peak_deg'))+'\n' for x in r)
subprocess.run([sys.argv[1],'replay'],input=text,text=True,check=True)

# These are observation-to-prediction fit checks, not independent validation.
def potential(a):
    a=abs(a)*math.pi/180;c=math.sqrt(.15**2-.005**2)
    dh=(-2*.12*math.sin(a/2)**2+.005*math.sin(a) if a<=math.asin(.005/.15)
        else .15-c+2*(c-.12)*math.sin(a/2)**2)
    return .1997*9.80665*dh*1000
x=[v['rate_dps']**2 for v in r];y=[potential(v['peak_deg']) for v in r]
xm=sum(x)/len(x);ym=sum(y)/len(y)
b=sum((a-xm)*(v-ym) for a,v in zip(x,y))/sum((a-xm)**2 for a in x);a=ym-b*xm
assert abs(a-.09046158)<1e-7 and abs(b-.00018973)<1e-8
loss=sum(potential(v['previous_deg'])-potential(v['peak_deg']) for v in r)/len(r)
assert abs(loss-.1401290743)<1e-6
print('32 measured unpowered half-cycles, fitted coefficients and mean effective decrease PASS')
