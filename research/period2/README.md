# Two-cycle rocking pattern: offline observation only

This isolated C++11 observer never requests or changes a motor command. It
describes one previously observed pattern, not the cause of rotation, ground
stride, walking safety, or an intervention that has been shown to work.

A pair consists of a candidate-strong-side peak followed by the next opposite
peak, with an individually matched, accepted preceding command decision for
each. Instantiate one observer per candidate strong side (+1 and -1). Four
consecutive complete pairs qualify only if:

* strong-side amplitudes alternate above / at-or-below stage target + 1.5°;
* after each overshoot the opposite side has an intentional zero-output coast
  decision and a peak no greater than stage target - 0.5°;
* on the other two pairs the opposite-side drive was confirmed sent;
* both decisions, both peaks, their time order and target are valid, with no
  missing outcome, duplicates, old data or target changes.

The thresholds are hypotheses chosen for the recorded 4.5° stage and are not
identified physical laws. Invalid/missing outcomes clear history, as do time
reversal, duplicate peaks and stale windows. The result also supplies a
`within_stage_plus_2p5` descriptive flag, false if any strong peak exceeds
stage target + 2.5°. This bound is not proof a pulse is safe or effective.

`replay_period2.py` uses the converter's saved peak/decision CSVs. It accepts
coast only when the logged reason is `VALID_NO_OUTPUT`, branch 15 and width 0;
unknown or unpaired decisions reset the appropriate observer. The recorded
peak timestamps are extrema times, **not** the later times when the firmware
confirmed those peaks. Thus the reported timestamps are retrospective and
optimistic; do not treat them as actual real-time detection times. They say
nothing about what would have happened after modifying an input. Different
firmware versions and changing setups are not randomized comparisons.

```
g++ -std=c++11 -Wall -Wextra -Werror test_period2_observer.cpp -o test_period2_observer
./test_period2_observer
python replay_period2.py ../run644
```

On the current local run644 replay, 18 overlapping windows are detected on
side -1. Their first and last extrema-time windows are outside the additional
stage + 2.5° bound; 16 windows lie inside it. The first qualifying window ends
at the 4.935 s recorded weak peak, not its unknown confirmation time. The
current firmware run305 yields zero detections. Older firmware runs 160 and
583 yield five and one overlapping windows; runs 329, 337 and 404 yield zero.
These are not independent events or evidence of false-positive rates.

All motor outputs, runner integration, any current/pulse limit changes,
hardware build and physical tests remain out of scope and unverified.
