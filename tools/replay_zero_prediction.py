"""Offline causal state sampling, followed by post-hoc zero-cross scoring.

Linear constant-rate prediction only, no output, contact claim or fitting.
"""
import argparse,bisect,csv,statistics
from pathlib import Path
parser=argparse.ArgumentParser();parser.add_argument('csv_dir',type=Path);a=parser.parse_args()
def read(n):
 with (a.csv_dir/n).open(encoding='utf8',newline='') as f:return list(csv.DictReader(f))
imu=[r for r in read('timeseries.csv') if r['state_id']=='3' and r['time_s'] and
     r['pitch_mekf_detector_relative_deg'] and r['physical_roll_rate_dps']]
imu.sort(key=lambda r:float(r['time_s']));tt=[float(x['time_s'])*1000 for x in imu]
cross=[int(r['zero_cross_time_ms']) for r in read('energy_control_autonomous_zero_cross_events.csv')
       if r['event_kind']=='energy_control_zero_cross' and r['valid']=='True']
cross.sort();errors=[];by_event={}
for f in read('foot_angles.csv'):
    if not (f['timestamp_valid']=='True' and f['frame_valid']=='True' and f['zero_ready']=='True' and
            f['left_valid']=='True' and f['right_valid']=='True' and f['measurement_time_us'] and
            f['frame_us'] and f['delivered_us'] and f['processing_us']):continue
    exposure=int(f['measurement_time_us'])/1000
    if exposure<0:continue
    now=exposure+(int(f['delivered_us'])-int(f['frame_us'])+int(f['processing_us']))/1000
    ix=bisect.bisect_right(tt,now)-1
    if ix<0 or now-tt[ix]>25:continue
    angle=float(imu[ix]['pitch_mekf_detector_relative_deg']);rate=float(imu[ix]['physical_roll_rate_dps'])
    if abs(rate)<1 or angle*rate>=0:continue
    eta=-angle/rate*1000
    if not 80<=eta<=300:continue
    j=bisect.bisect_right(cross,now)
    if j==len(cross) or cross[j]-now>500:continue
    event=cross[j];prediction=now+eta
    # Retain the first eligible prediction before each future recorded event.
    if event not in by_event:by_event[event]=(now,prediction,prediction-event)
errors=[x[2] for x in by_event.values()]
print('matched crossings',len(errors),'of',len(cross))
if errors:
 print('prediction error ms; median signed',round(statistics.median(errors),1),
       'median absolute',round(statistics.median(abs(x) for x in errors),1),
       'range',round(min(errors),1),round(max(errors),1))
 for t,(now,p,e) in list(by_event.items())[:8]:
  print('known at ms',round(now,1),'predicted zero ms',round(p,1),'recorded zero ms',t,'error ms',round(e,1))
