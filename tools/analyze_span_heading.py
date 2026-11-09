#!/usr/bin/env python3
"""Exploratory foot-span versus gyro-heading analysis of decoded RWLOG CSVs.

Uses only first reported eligible span for each exact window. The same-window
statistics are retrospective, not a causal control signal. Available-event
lag and future heading are separately reported; heading drifts without a
magnetometer and recorded frame extrema do not establish ground displacement.
"""
import argparse
import bisect
import csv
import json
import math
import statistics
from pathlib import Path


def read(path):
    with path.open(encoding='utf-8-sig', newline='') as f:
        return list(csv.DictReader(f))


def corr(pairs):
    if len(pairs) < 3:
        return None
    x, y = zip(*pairs)
    mx, my = statistics.mean(x), statistics.mean(y)
    sx = sum((v-mx)**2 for v in x)
    sy = sum((v-my)**2 for v in y)
    return round(sum((a-mx)*(b-my) for a,b in pairs) / math.sqrt(sx*sy), 4) if sx and sy else None


def analyze(root):
    metadata = json.loads((root / 'metadata.json').read_text(encoding='utf-8-sig'))
    spans = {}
    for row in read(root / 'energy_control_autonomous_zero_cross_events.csv'):
        if row.get('foot_span_valid') != 'True' or row.get('event_kind') != 'energy_control_zero_cross':
            continue
        try:
            a, b, event = (int(row[k]) for k in ('foot_span_start_ms', 'foot_span_end_ms', 'zero_cross_time_ms'))
            left, right = (float(row[k]) for k in ('foot_span_left_deg', 'foot_span_right_deg'))
        except (TypeError, ValueError):
            continue
        if a <= 2000 or not 0 < b-a < 1100 or event < b or not all(math.isfinite(v) for v in (left,right)):
            continue
        key = (a,b)
        if key not in spans or event < spans[key]['first_seen_ms']:
            spans[key] = dict(start_ms=a,end_ms=b,first_seen_ms=event,left_deg=left,right_deg=right,span_left_minus_right_deg=left-right)

    imu = []
    for row in read(root / 'timeseries.csv'):
        if row.get('state_id') != '3' or row.get('gyro_heading_valid') != '1':
            continue
        try:
            t, yaw = float(row['time_s'])*1000, float(row['gyro_heading_deg'])
        except (KeyError, TypeError, ValueError):
            continue
        if math.isfinite(t) and math.isfinite(yaw):
            imu.append((t,yaw))
    imu.sort(); times = [x[0] for x in imu]

    def change(a,b):
        i, j = bisect.bisect_left(times,a), bisect.bisect_right(times,b)-1
        if i >= len(times) or j < i or times[i]-a > 25 or b-times[j] > 25:
            return None
        return round(imu[j][1]-imu[i][1],4)

    out=[]
    for span in sorted(spans.values(),key=lambda x:x['start_ms']):
        a,b,k = span['start_ms'],span['end_ms'],span['first_seen_ms']
        length = b-a
        span['first_seen_after_end_ms']=k-b
        span['yaw_same_window_deg']=change(a,b)
        span['yaw_previous_shifted_window_deg']=change(a-length,b-length)
        span['yaw_next_shifted_window_deg']=change(a+length,b+length)
        span['yaw_future_after_first_report_deg']=change(k,k+length)
        out.append(span)
    def getpairs(key):
        return [(x['span_left_minus_right_deg'],x[key]) for x in out if x[key] is not None]
    return {'run':str(root),'firmware_revision':metadata.get('firmware_revision'),
            'selection':'first zero-cross event for each unique valid foot-span window; start>2s; 0<duration<1.1s; event>=end; gyro valid, time within 25ms at both edges',
            'definition':'left minus right per-cycle camera-relative angular span; yaw is 3-axis integrated gyro heading change, no magnetometer; same-window is retrospective',
            'count':len(out),'correlations':{k:{'n':len(getpairs(k)),'pearson_r':corr(getpairs(k))} for k in ('yaw_same_window_deg','yaw_previous_shifted_window_deg','yaw_next_shifted_window_deg','yaw_future_after_first_report_deg')},
            'cycles':out}


if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('dirs',type=Path,nargs='+');args=p.parse_args()
    print(json.dumps([analyze(d) for d in args.dirs],indent=2,ensure_ascii=False))
