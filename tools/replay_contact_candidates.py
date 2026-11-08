"""Causal camera/IMU geometry candidate replay, with retrospective scoring only.

Research probe: equal sole height is not necessarily contact or support transfer.
The recorded delivery/processing time is a lower bound on availability.
"""
import bisect
import csv
import json
import statistics
import sys
from collections import Counter
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from contact_geometry_probe import switch


def rows(path):
    with path.open(encoding='utf8', newline='') as f:
        return list(csv.DictReader(f))


def robust_target(angle, roots, margin=0.25, max_spread=1.5):
    """Pick a late geometric threshold only when all hypotheses agree on side.

    The threshold is deliberately closest to zero. All roots must be ahead of
    the current angle. No guarantee exists outside these model hypotheses.
    """
    if not roots or any(r is None for r in roots):
        return None, 'missing_root'
    side = 1 if angle > 0 else -1
    directed = [side * r for r in roots]
    if min(directed) <= margin:
        return None, 'sign_or_margin'
    if max(directed) >= abs(angle):
        return None, 'threshold_already_passed'
    if max(directed) - min(directed) > max_spread:
        return None, 'model_spread'
    return side * min(directed), 'accepted'


def replay(directory, calibrated=False):
    imu = [r for r in rows(directory / 'timeseries.csv') if r['state_id'] == '3'
           and r['time_s'] and r['pitch_mekf_detector_relative_deg']
           and r['physical_roll_rate_dps'] and r['roll_mekf_abs_deg']]
    imu.sort(key=lambda r: float(r['time_s']))
    t = [1000 * float(r['time_s']) for r in imu]
    initial_pitch = float(imu[0]['roll_mekf_abs_deg'])
    counts, candidates = Counter(), []
    for f in rows(directory / 'foot_angles.csv'):
        if any(f[k] != 'True' for k in ('timestamp_valid', 'frame_valid', 'zero_ready',
                                       'left_valid', 'right_valid', 'left_in_range', 'right_in_range')):
            continue
        if not all(f[k] for k in ('measurement_time_us', 'frame_us', 'delivered_us',
                                  'processing_us', 'left_deg', 'right_deg')):
            continue
        exposure = int(f['measurement_time_us']) / 1000
        latency = (int(f['delivered_us']) - int(f['frame_us']) + int(f['processing_us'])) / 1000
        if exposure < 0 or latency < 0:
            continue
        now = exposure + latency
        i = bisect.bisect_right(t, now) - 1
        if i < 0 or now - t[i] > 25:
            continue
        angle = float(imu[i]['pitch_mekf_detector_relative_deg'])
        rate = float(imu[i]['physical_roll_rate_dps'])
        if abs(rate) < 1 or angle * rate >= 0:
            continue
        eta = -angle / rate * 1000
        if not 80 <= eta <= 300:
            continue
        counts['eligible_frames'] += 1
        pitch = float(imu[i]['roll_mekf_abs_deg']) - initial_pitch
        left, right = float(f['left_deg']), float(f['right_deg'])
        if calibrated:
            # run305 only: provisional table origin, not confirmed mechanical zero.
            # Model assumes calibrated forward-positive equals positive X rotation;
            # sign relation between detector/body pitch and model is still unknown.
            left += 1.246
            right += 1.784
            roots = [switch(sp * pitch, left, right, h)
                     for sp in (-1, 1) for h in (55.33, 82.83, 120.0)]
        else:
            roots = [switch(sp * pitch, sl * left, sr * right, h)
                     for sp in (-1, 1) for sl in (-1, 1) for sr in (-1, 1)
                     for h in (55.33, 82.83, 120.0)]
        threshold, reason = robust_target(angle, roots)
        counts[reason] += 1
        if threshold is not None:
            candidates.append(dict(now=now, angle=angle, rate=rate, eta=eta,
                                   threshold=threshold, prediction=now + (threshold-angle)/rate*1000,
                                   root_min=min(roots), root_max=max(roots)))
    # The ground-truth event stream is deliberately first read here, after
    # every candidate has been formed solely from samples available by now.
    crosses = sorted(int(r['zero_cross_time_ms']) for r in rows(directory / 'energy_control_autonomous_zero_cross_events.csv')
                     if r['event_kind'] == 'energy_control_zero_cross' and r['valid'] == 'True')
    per_event = {}
    for c in candidates:
        j = bisect.bisect_right(crosses, c['now'])
        if j == len(crosses) or crosses[j] - c['now'] > 500:
            continue
        cross = crosses[j]
        if cross in per_event:
            continue
        start = bisect.bisect_left(t, c['now'])
        end = bisect.bisect_left(t, cross)
        side = 1 if c['angle'] > 0 else -1
        reached = next((t[k] for k in range(start, end)
                        if side * float(imu[k]['pitch_mekf_detector_relative_deg']) <= side * c['threshold']), None)
        per_event[cross] = dict(known_ms=round(c['now'], 1), threshold_deg=round(c['threshold'], 3),
                                predicted_ms=round(c['prediction'], 1), measured_reach_ms=reached,
                                lead_ms=round(cross - reached, 1) if reached is not None else None,
                                zero_prediction_error_ms=round(c['now'] + c['eta'] - cross, 1))
    return dict(directory=str(directory), calibrated=calibrated, counts=dict(counts), recorded_valid_zero_events=len(crosses),
                events_with_candidate=len(per_event), scored_events=per_event,
                caveat='Candidate threshold is conditional on unverified geometry/signs; availability is a lower bound; no motor command sent.')


if __name__ == '__main__':
    calibrated = '--calibrated-run305' in sys.argv
    paths = [p for p in sys.argv[1:] if p != '--calibrated-run305']
    if calibrated and any(Path(p).name != 'run305' for p in paths):
        raise ValueError('the calibration offsets are specific to run305')
    print(json.dumps([replay(Path(p), calibrated) for p in paths], ensure_ascii=False, indent=2))
