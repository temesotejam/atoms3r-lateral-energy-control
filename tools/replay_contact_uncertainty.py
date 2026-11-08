import argparse, bisect, csv, json, statistics
from pathlib import Path
from contact_geometry_probe import switch

parser=argparse.ArgumentParser(description='Hypothesis-only geometry and causal camera replay; does not identify ground contact.')
parser.add_argument('csv_dir',type=Path,help='directory produced by the RWLOG CSV converter')
args=parser.parse_args()
with (args.csv_dir/'foot_calibration.json').open(encoding='utf8') as source:
    calibration=json.load(source)
def interpolated_angle(points,x):
    if x<=points[0][0]:i=0
    elif x>=points[-1][0]:i=len(points)-2
    else:i=next(j for j in range(len(points)-1) if points[j][0]<=x<=points[j+1][0])
    x0,y0=points[i];x1,y1=points[i+1]
    return y0+(x-x0)*(y1-y0)/(x1-x0)
try:
    cal=calibration['calibration']
    offsets={side:interpolated_angle(cal[side+'_points'],float(calibration[side+'_zero_x']))
             for side in ('left','right')}
except (KeyError,TypeError,ValueError,StopIteration,ZeroDivisionError):
    offsets=None
def read(name):
    with (args.csv_dir/name).open(encoding='utf8',newline='') as f:return list(csv.DictReader(f))
frames=[r for r in read('foot_angles.csv') if r['frame_valid']=='True' and r['timestamp_valid']=='True' and r['zero_ready']=='True' and r['left_valid']=='True' and r['right_valid']=='True' and
        r['left_in_range']=='True' and r['right_in_range']=='True' and r['measurement_time_us'] and
        r['left_deg'] and r['right_deg'] and int(r['measurement_time_us'])>=0]
ts=[r for r in read('timeseries.csv') if r['state_id']=='3' and r['time_s'] and r['roll_mekf_abs_deg']]
times=[float(r['time_s']) for r in ts]
events=[r for r in read('energy_control_autonomous_zero_cross_events.csv') if r['event_kind']=='energy_control_zero_cross' and int(r['zero_cross_time_ms'])>2000]
stats=[]
for e in events:
    t=int(e['zero_cross_time_ms'])
    # Conservative recorded delivery time, rather than future camera samples.
    possible=[r for r in frames if r['processing_us'] and int(r['measurement_time_us'])/1000+
              (int(r['delivered_us'])-int(r['frame_us'])+int(r['processing_us']))/1000<=t-80]
    if not possible:continue
    f=possible[-1];tm=int(f['measurement_time_us'])/1000
    if t-tm>300:continue
    k=bisect.bisect_right(times,tm/1000)-1
    if k<0 or tm/1000-times[k]>.025:continue
    pitch=float(ts[k]['roll_mekf_abs_deg'])-float(ts[0]['roll_mekf_abs_deg'])
    left,right=float(f['left_deg']),float(f['right_deg'])
    # Sign and scale are uncalibrated; compare hypotheses, not physical claims.
    roots=[switch(sp*pitch,sl*left,sr*right) for sp in (-1,1) for sl in (-1,1) for sr in (-1,1)]
    roots=[x for x in roots if x is not None]
    if roots:stats.append((t,round(tm),round(left,2),round(right,2),round(pitch,2),round(min(roots),2),round(max(roots),2)))
print('eligible crossings:',len(stats),'/',len(events))
print('boot-to-table zero offsets [deg], not verified mechanical center:',offsets)
for x in stats[:12]:print('cross ms, exposure ms, left, right, body pitch, min/max candidate deg:',*x)
if stats: print('candidate spans: median',round(statistics.median(x[-1]-x[-2] for x in stats),2),'deg')
