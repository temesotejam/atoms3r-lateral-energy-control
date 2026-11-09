# Foot-span and heading exploration

Run from the repository workspace after decoding the given RWLOGs to CSV:

```
python tools/analyze_span_heading.py work/run305 work/run337 work/run329 work/run404 work/run160
```

For each distinct `(foot_span_start_ms, foot_span_end_ms)`, retain the first
normal `energy_control_zero_cross` event that reports `foot_span_valid=True`.
Require the window to start after 2000 ms, have a positive duration under
1100 ms, and already have ended at its reporting event. Multiple zero events
can repeat a single span and must not be counted independently. These filters
select the reported-valid subset rather than all cycles.

The foot quantity is **left minus right recorded relative marker-angle span**
(degrees). Heading is the change in the 3-axis integrated gyro heading in
the specified time interval. Do not infer ground-relative foot displacement or
contact from span, and do not infer true external yaw without verification:
the heading is uncorrected by magnetometer and can drift, and its physical
sign has not been independently checked in these runs. Match both heading
endpoints within 25 ms to logged IMU rows marked valid. A valid endpoint does
not establish perfect continuity between endpoints.

Compare the same-window association with equal-duration windows shifted by
one cycle and with the equal-duration interval **after the first event reporting
the span**. The latter is a simple availability-aware negative-control check,
not a predictive model or an evaluation of the motor interventions, because
other state and control changes intervene. Runs have different firmware:
run305 is 1.0.99, run337 is 1.0.98, run329 is 1.0.97, run404 is 1.0.96,
and run160 is 1.0.95. Correlations are exploratory with small, selected,
serially dependent samples; they should not be treated as causality or a
control gain.

For run305 the normal-zero-only selection retains 7 windows; the same-window
Pearson coefficient is about -0.918, but the window after the first report
gives +0.689. The latter sign is not a recommendation: it highlights why
using retrospective same-window correlations to select an online correction
can fail. The earlier eight-window figure also included a stall-rescue event.
