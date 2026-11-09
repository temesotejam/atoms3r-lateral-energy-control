"""Offline, retrospective evaluation only; never commands the actuator.

The last usable camera time is an *optimistic* lower bound in these legacy
logs. Geometric equality of minimum sole heights is not actual contact.
"""
import argparse
import bisect
import csv
import json
import math
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from contact_geometry_probe import switch


def read(path):
    with path.open(encoding='utf8', newline='') as f:
        return list(csv.DictReader(f))


def threshold(angle, roots, max_spread=1.5, margin=.25):
    if not math.isfinite(angle) or not roots or any(x is None or not math.isfinite(x) for x in roots): return None
    if angle == 0: return None
    side = 1 if angle > 0 else -1
    p = [side*x for x in roots]
    if min(p) <= margin or max(p)-min(p) > max_spread: return None
    return side*min(p)


def should_fire(angle, rate, side, target, fired):
    return (target is not None and math.isfinite(target) and math.isfinite(angle) and math.isfinite(rate)
            and not fired and side in (-1, 1)
            and side*angle > 0 and side*rate < 0 and side*angle <= side*target)


def runner_rate(row):
    """Approximate the real v1.0.99 detector rate from quantized log fields."""
    return (float(row['gy_dps'])-float(row['mekf_bias_y_dps'])) * 0.908911


def run(path, left_offset, right_offset, legacy_rate=False):
    meta = json.loads((path/'metadata.json').read_text(encoding='utf8'))
    if (meta.get('energy_control_autonomous_target_peak_deg') != 8.0
            or meta.get('autonomous_input_peak_percent') != 0.0):
        raise ValueError('offline prototype is scoped to target 8deg, nominal zero input timing')
    raw = [r for r in read(path/'timeseries.csv') if r['state_id']=='3'
           and r['time_s'] and r['pitch_mekf_detector_relative_deg']
           and r['roll_mekf_abs_deg'] and r.get('gy_dps') and r.get('mekf_bias_y_dps')
           and (not legacy_rate or r.get('physical_roll_rate_dps'))]
    raw.sort(key=lambda r: float(r['time_s']))
    imu = [(float(r['time_s'])*1000, float(r['pitch_mekf_detector_relative_deg']),
            float(r['physical_roll_rate_dps']) if legacy_rate else runner_rate(r),
            float(r['roll_mekf_abs_deg'])) for r in raw]
    imu = [r for r in imu if all(math.isfinite(v) for v in r)]
    if not imu: raise ValueError('no finite IMU samples')
    pitch0=imu[0][3]
    frames=[]
    for f in read(path/'foot_angles.csv'):
        if any(f.get(k)!='True' for k in ('timestamp_valid','frame_valid','zero_ready',
                    'left_valid','right_valid','left_in_range','right_in_range')): continue
        keys=('measurement_time_us','frame_us','delivered_us','processing_us','left_deg','right_deg')
        if not all(f.get(k) for k in keys): continue
        t=int(f['measurement_time_us'])/1000
        latency=(int(f['delivered_us'])-int(f['frame_us'])+int(f['processing_us']))/1000
        if t<0 or latency<0: continue
        l,r=float(f['left_deg'])+left_offset,float(f['right_deg'])+right_offset
        if not math.isfinite(l) or not math.isfinite(r): continue
        frames.append((t+latency,l,r))
    frames.sort()
    # Both streams are stepped forward. Zero events are not loaded until later.
    events=[]; cur=0; target=None; target_expiry=0; side=0; fired=False
    for i,(t,a,v,p) in enumerate(imu):
        if side and side*a <= 0:
            target=None; fired=False; side=0
        if target is not None and t>target_expiry:
            target=None
        while cur<len(frames) and frames[cur][0] <= t:
            ft,l,r=frames[cur];cur+=1
            if t-ft>25 or abs(v)<1 or a*v>=0: continue
            h=[switch(sign*(p-pitch0),l,r,H) for sign in (-1,1) for H in (55.33,82.83,120.)]
            q=threshold(a,h)
            if q is None: continue
            this_side=1 if a>0 else -1
            if side and side!=this_side: continue
            if not side: side=this_side
            target=q
            target_expiry=ft+100
        if should_fire(a,v,side,target,fired):
            events.append({'time_ms':t,'angle_deg':a,'target_deg':target,'side':side})
            fired=True
    zeros=sorted((int(r['zero_cross_time_ms']), int(r['previous_peak_side'])) for r in read(path/'energy_control_autonomous_zero_cross_events.csv')
                 if r['event_kind']=='energy_control_zero_cross' and r['valid']=='True')
    zero_times=[z[0] for z in zeros]
    matches={}
    for e in events:
        j=bisect.bisect_right(zero_times,e['time_ms'])
        if j<len(zeros) and zeros[j][1]==e['side'] and zeros[j][0]-e['time_ms']<250 and zeros[j][0] not in matches:
            matches[zeros[j][0]]=round(zeros[j][0]-e['time_ms'],1)
    return {'rate_source':'physical_roll_rate_dps (legacy)' if legacy_rate else '(gy_dps-mekf_bias_y_dps)*0.908911 (quantized logged fields)',
            'frames':len(frames),'early_events':len(events),'valid_zeros':len(zeros),
            'matched':len(matches),'lead_ms':list(matches.values()), 'events':events,
            'warning':'No contact truth. Foot joint coordinates and offsets assumed; camera availability is optimistic. Not a motor controller.'}


if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('folder',type=Path);p.add_argument('--left-offset',type=float,default=0);p.add_argument('--right-offset',type=float,default=0);p.add_argument('--legacy-rate',action='store_true');a=p.parse_args()
    print(json.dumps(run(a.folder,a.left_offset,a.right_offset,a.legacy_rate),indent=2))
