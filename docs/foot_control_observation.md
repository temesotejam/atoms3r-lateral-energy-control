# Foot frame timing diagnostic

`foot_control_observations` records the time immediately after the control task
returns from the camera snapshot and **before** `runner.observeFootFrame`.
It does not claim that foot state was applied by that timestamp. Each sparse
row is `[foot_frames row index, control_observed_us - delivered_us]` in
microseconds. An omitted row means unknown: it can reflect truncation, a frame
never selected by the control task, or an unmatched publication race. Never
infer a timestamp for a missing row. `attempts` counts same-run observations;
`unmatched` counts those which could not safely match the most recently stored
camera frame. At most 64 recorded observations are selected over the full run;
`total`, `stored`, and `truncated` disclose sampling. A race may leave even the
last frame without a time.

This diagnostic does not change the motor command. Camera exposure semantics,
real-device timing overhead, and actual unmatched rates have not been verified.
Old logs without the table remain supported.

The status UI and RWLOG metadata carry the diagnostic build ID
`foot-control-observation-v1`, separate from the unchanged firmware revision.
