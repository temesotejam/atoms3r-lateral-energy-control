#!/usr/bin/env python3
"""Compare decoded RWLOG camera and control diagnostics; does not command hardware.

Input is one or more directories produced by convert_rwlog_to_csv.py. Frame
sequence gaps and CSV marker rates describe recorded frames only: fast capture
experiments can intentionally decimate these rows. No source timestamp is
assumed to equal the actual exposure time.
"""
import argparse
import csv
import json
import math
import statistics
from pathlib import Path


def num(v):
    try:
        value = float(v)
        return value if math.isfinite(value) else None
    except (ValueError, TypeError):
        return None


def istrue(value):
    return str(value).strip().lower() in ('true', '1')


def stats(values):
    a = sorted(v for v in values if v is not None and math.isfinite(v))
    if not a:
        return {'count': 0, 'median': None, 'p90': None, 'max': None}
    def pct(f):
        pos = (len(a)-1)*f
        i = int(pos)
        return a[i] + (a[min(i+1,len(a)-1)]-a[i])*(pos-i)
    return {'count': len(a), 'median': round(statistics.median(a), 3),
            'p90': round(pct(.9), 3), 'max': round(a[-1], 3)}


def read_csv(path):
    if not path.exists(): return []
    with path.open(newline='', encoding='utf-8-sig') as f:
        return list(csv.DictReader(f))


def read_json(path):
    if not path.exists(): return {}
    with path.open(encoding='utf-8-sig') as f:
        obj = json.load(f)
        return obj if isinstance(obj, dict) else {}


def rate(rows, key):
    # Exclude empty values instead of calling unknown observations failures.
    valid = [r[key] for r in rows if r.get(key, '') != '']
    return {'valid': sum(istrue(v) for v in valid), 'known': len(valid),
            'percent': round(100*sum(istrue(v) for v in valid)/len(valid),2) if valid else None}


def diffs(rows, field, scale):
    out = []
    for prev, cur in zip(rows, rows[1:]):
        a, b = num(prev.get(field)), num(cur.get(field))
        if a is not None and b is not None and b > a:
            out.append((b-a)*scale)
    return out


def camera_subset(rows):
    good = [r for r in rows if istrue(r.get('timestamp_valid')) and istrue(r.get('frame_valid'))]
    seq = [int(v) for r in rows if (v := num(r.get('sequence'))) is not None and v >= 0 and v.is_integer()]
    gaps = [b-a-1 for a,b in zip(seq,seq[1:]) if b>a+1]
    latency, processing, lower_bound, control, age = [], [], [], [], []
    for r in good:
        frame, delivered, duration, observed = (num(r.get(k)) for k in ('frame_us','delivered_us','processing_us','control_observed_us'))
        if duration is not None and duration >= 0: processing.append(duration/1000)
        if frame is not None and delivered is not None and delivered >= frame:
            latency.append((delivered-frame)/1000)
            if duration is not None and duration >= 0: lower_bound.append((delivered-frame+duration)/1000)
            if observed is not None and observed >= delivered:
                control.append((observed-delivered)/1000)
                age.append((observed-frame)/1000)
    return {'recorded_rows': len(rows), 'valid_timestamp_and_frame_rows': len(good),
            'recorded_sequence_gap_events': len(gaps), 'recorded_sequence_missing_count': sum(gaps),
            'recorded_frame_interval_ms': stats(diffs(good,'frame_us',.001)),
            'frame_to_delivery_ms': stats(latency), 'processing_ms': stats(processing),
            'frame_to_processing_end_lower_bound_ms': stats(lower_bound),
            'delivery_to_control_observed_ms': stats(control), 'frame_to_control_observed_ms': stats(age),
            'frame_valid_rate_recorded': rate(rows,'frame_valid'),
            'left_valid_rate_recorded': rate(rows,'left_valid'), 'right_valid_rate_recorded': rate(rows,'right_valid')}


def summarize(directory):
    p = Path(directory)
    rows, ts = read_csv(p/'foot_angles.csv'), read_csv(p/'timeseries.csv')
    meta, head = read_json(p/'metadata.json'), read_json(p/'header.json')
    foot = meta.get('foot_observation') or {}
    # Metadata can be absent or from an older/newer revision.
    if not isinstance(foot,dict): foot={}
    profile = meta.get('control_work_profile') or {}
    control = meta.get('control_latency') or {}
    if not isinstance(profile,dict): profile={}
    if not isinstance(control,dict): control={}
    series = [r for r in ts if str(r.get('state_id','')) == '3']
    update = [v/1000 for r in series if (v:=num(r.get('imu_update_dt_us'))) is not None and v>0]
    ages = [v/1000 for r in series if (v:=num(r.get('imu_sample_age_us'))) is not None and v>=0]
    counts = {k:foot.get(k) for k in ('target_fps','capacity','count','overflow','fast_capture_experiment_mode','fast_capture_active','fast_record_skipped','fast_run_frames','fast_run_first_us','fast_run_last_us','fast_copy_max_us','fast_run_received','fast_run_invalid_frame','fast_run_right_invalid','fast_run_left_invalid','fast_control_observed','fast_control_max_gap_us')}
    first, last, total = (num(foot.get(k)) for k in ('fast_run_first_us','fast_run_last_us','fast_run_frames'))
    counts['fast_run_mean_fps_from_metadata'] = round((total-1)*1e6/(last-first),3) if first is not None and last is not None and total is not None and total>1 and last>first else None
    received = num(foot.get('fast_run_received'))
    for label,key in [('frame','fast_run_invalid_frame'),('right_marker','fast_run_right_invalid'),('left_marker','fast_run_left_invalid')]:
        invalid = num(foot.get(key))
        counts['fast_'+label+'_valid_rate_percent'] = (round(100*(received-invalid)/received,2)
            if received is not None and received>0 and invalid is not None and 0<=invalid<=received else None)
    return {'path':str(p),'firmware_revision':meta.get('firmware_revision'),'header':{k:head.get(k) for k in ('run_id','imu_period_ms','log_period_ms','sample_count')},
            'camera_metadata':counts,
            'camera_recorded_all_states':camera_subset(rows),
            'camera_recorded_measurement_state_3':camera_subset([r for r in rows if str(r.get('state_id',''))=='3']),
            'diagnostic_observation_table':{k:(meta.get('foot_control_observations') or {}).get(k) for k in ('total','stored','truncated')},
            'imu_measurement_state_3':{'recorded_rows':len(series), 'imu_update_dt_ms':stats(update),
                 'imu_update_dt_over_5ms_recorded_count':sum(v>5 for v in update), 'imu_sample_age_ms':stats(ages),
                 'log_row_interval_ms':stats(diffs(series,'log_time_s',1000))},
            'control_measurement_profile':{'pulse_off_owner_iteration':profile.get('pulse_off',{}).get('owner_iteration') if isinstance(profile.get('pulse_off'),dict) else None,
                 'pulse_on_owner_iteration':profile.get('pulse_on',{}).get('owner_iteration') if isinstance(profile.get('pulse_on'),dict) else None,
                 'control_ready_age':control.get('control_ready_age'),'done_age':control.get('done_age'),'io_failures':control.get('io_failures')},
            'caveats':['Recorded frame statistics exclude any intentionally unsaved or lost frames; fast metadata mean fps is a run-wide aggregate, not the frame interval distribution.',
              'Recorded row sequence gaps are gaps in saved data and are not automatically capture failures.',
              'frame_us is a driver timestamp, not verified optical exposure time; delivered+processing excludes publication and polling.',
              'control_observed timestamps may be sparse, capped or missing; latency statistics use only known saved rows.',
              'Frame and marker rates are relative to rows with known flags; no ground truth accuracy is implied.',
              'Logged IMU dt/age are snapshots and may be repeated; log row spacing is not the controller iteration period, and state filtering can join separate measurement segments.',
              'Control-profile iteration is work duration (includes preemption), not start-to-start control period.']}


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('dirs',nargs='+',help='decoded RWLOG CSV directories')
    args=parser.parse_args()
    print(json.dumps([summarize(d) for d in args.dirs],indent=2,ensure_ascii=False,allow_nan=False))

if __name__=='__main__': main()
