"""Compare tentative geometric triggers over boot-zero assumptions.

Post-hoc ZERO is used only for scoring. This cannot predict contact or the
counterfactual effect of issuing a real command.
"""
import bisect
import csv
import json
import sys
from pathlib import Path
from batch_foot_timing import offsets
from causal_candidate_replay import run


def evaluate(path, min_lead_ms=10):
    left, right = offsets(path)
    with (path / 'energy_control_autonomous_zero_cross_events.csv').open(encoding='utf8', newline='') as f:
        zero = sorted((int(x['zero_cross_time_ms']), int(x['previous_peak_side']))
                      for x in csv.DictReader(f)
                      if x['event_kind'] == 'energy_control_zero_cross' and x['valid'] == 'True')
    times = [x[0] for x in zero]
    scenarios = []
    for delta in (-1., 0., 1.):
        result = run(path, left+delta, right+delta,
                     observed_subset=True, use_control_observed=True)
        match = {}
        for e in result['events']:
            i = bisect.bisect_right(times, e['time_ms'])
            if i < len(zero) and zero[i][1] == e['side']:
                lead = zero[i][0] - e['time_ms']
                if 0 < lead < 250:
                    match.setdefault(zero[i][0], lead)
        scenarios.append(match)
    common = set(scenarios[0]).intersection(*scenarios[1:])
    return {'common_zero_ms': sorted(common),
            'lead_ms_by_zero': {str(z):[round(s[z], 3) for s in scenarios] for z in sorted(common)},
            'common_with_min_lead_ms': sorted(z for z in common if min(s[z] for s in scenarios) >= min_lead_ms),
            'min_lead_ms':min_lead_ms,
            'warning':'Assumed geometry and boot offsets, sparse frames, approximate clocks; not physical contact or actuation.'}


if __name__ == '__main__':
    print(json.dumps(evaluate(Path(sys.argv[1])), indent=2))
