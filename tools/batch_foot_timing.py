"""Retrospective sensitivity study; no control output or contact truth."""
import json
import sys
from pathlib import Path
from causal_candidate_replay import run


def interpolate(points, x):
    pts = sorted((float(a), float(b)) for a, b in points)
    if len(pts) < 2 or not pts[0][0] <= x <= pts[-1][0]:
        raise ValueError('boot reference outside calibration points')
    for (x0, y0), (x1, y1) in zip(pts, pts[1:]):
        if x0 <= x <= x1:
            return y0 + (x-x0) * (y1-y0) / (x1-x0)
    raise ValueError('invalid calibration table')


def offsets(folder):
    data = json.loads((folder / 'foot_calibration.json').read_text(encoding='utf8'))
    return tuple(interpolate(data['calibration'][side+'_points'], float(data[side+'_zero_x']))
                 for side in ('left', 'right'))


def main():
    output = []
    for name in sys.argv[1:]:
        folder = Path(name)
        try:
            left, right = offsets(folder)
            scenarios = []
            for delay, aligned in ((0, False),(0, True),(20, True),(50, True)):
                r = run(folder, left, right, extra_delay_ms=delay,align_frame_pose=aligned)
                scenarios.append({k:r[k] for k in ('extra_delay_ms','frame_pose_aligned','frames','early_events',
                    'previous_recorded_zero_guard_blocked','screened_events','valid_zeros','matched','screened_matches')})
            output.append({'run':folder.name,'assumed_offsets_deg':[round(left,3),round(right,3)],
                           'scenarios':scenarios})
        except (ValueError, KeyError, FileNotFoundError) as exc:
            output.append({'run':folder.name, 'error':str(exc)})
    print(json.dumps(output, ensure_ascii=False, indent=2))


if __name__ == '__main__':
    main()
