# Offline late foot-geometry candidate replay

This is a research-only replay. It produces no motor command and does not
implement or validate the firmware's normal ZERO fallback. Do not connect it to
physical output on the strength of this experiment.

```
python test/test_late_candidate_replay.py
python tools/late_candidate_replay.py work/run305 --left-offset 1.246 --right-offset 1.784
```

It accepts only logs tagged target amplitude 8 degrees and zero-percent timing,
the current offline prototype's intended scope. It advances through IMU and
camera frames chronologically. At each camera-availability estimate it solves
the geometric equal-height condition for **three hypothesized hip heights**
(55.33, 82.83 and 120 mm) and both assumed body-pitch signs. Foot angle is
forward-positive only under the documented but not independently verified model
mapping, and the requested angle offsets are assumptions. It accepts a
threshold only when all six hypotheses agree on the pre-zero side within 1.5
degrees. Thresholds expire 100 ms after the frame availability estimate.

Unlike the earlier replay, if the selected threshold is already past by the
time the frame is assumed available, this replay permits an immediate candidate
when the sampled angle remains on the pre-ZERO side and velocity points inward.
A candidate is rejected at or beyond sampled ZERO and cannot repeat until the
angle crosses. The later ZERO event stream is loaded only for scoring. These
sampled guards do not establish that a real motor command could be sent before
the physical crossing. The real firmware's minimum-half-cycle, command-delivery,
coasting, faults and one-shot state transitions are not simulated.

Old logs do not record when the control task actually obtained each frame.
`frame_us + (delivered_us - frame_us) + processing_us` is only an optimistic
availability lower bound; frame timestamp vs. optical exposure is not verified.
The sampled foot and IMU do not correspond to the same instant. The next ZERO
event is matched retrospectively only if it is within 250 ms and has the same
previous-peak side. The model's equal minimum sole heights are **not ground
truth contact or load transfer**; no contact label or motor response is measured.

On run305, the documented provisional table-origin offsets yield 21 matched
candidates of 34 valid recorded ZERO events, with retrospective leads of
6–143 ms. Changing both assumed offsets by −1 degree yields 14, and by +1 degree
yields 30. These are one-log sensitivity observations, not success rates or
evidence that any candidate improves walking.
In particular, one run305 candidate occurred 1 ms after the preceding recorded
ZERO and would be blocked if that event were the last accepted crossing under
the firmware's 250 ms minimum-half-cycle rule. This is another reason why the
offline counts must not be treated as commands that the firmware would execute.
