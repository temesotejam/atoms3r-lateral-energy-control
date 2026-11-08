# Camera and control comparison from decoded logs

Run from the repository root after decoding each `.rwlog` with the matching
`tools/convert_rwlog_to_csv.py`:

```
python tools/camera_metrics.py path/to/baseline_csv path/to/trial_csv
python tools/test_camera_metrics.py
```

The JSON report separates all saved foot rows from `state_id == 3` measurement
rows, reports medians, linearly interpolated p90 and maximum in milliseconds,
and includes firmware revision, experimental metadata, validity rates, saved
sequence gaps, IMU timing snapshots, and control-work/latency aggregates.
Unknown files, empty series and missing optional columns produce null or zero
sample counts, not invented zero-duration measurements. Metric names explicitly
identify data subsets and units.

**Interpretation:**

- `frame_us` is a camera-driver timestamp, not verified optical exposure.
  `delivered_us + processing_us` omits publication and polling, and is an
  optimistic lower bound on time usable by control.
- The control-observed timestamp may only exist on a limited sample of saved
  frames. Missing timestamps remain unknown; the statistics can have selection
  bias and cannot describe every live observation.
- The 15 Hz logging gate can deliberately omit fast images. CSV intervals,
  sequence gaps and detection rates describe saved rows, not full-rate capture
  or live control. Metadata fields `fast_run_received`, `fast_run_invalid_frame`,
  `fast_run_left_invalid`, `fast_run_right_invalid`, `fast_control_observed` and
  `fast_control_max_gap_us`, if present, describe experiment-wide aggregates;
  the side-invalid counters include invalid images. The first/last valid-frame
  timestamps and frame count give an average, not a distribution.
- IMU update dt and age are sampled in logged rows and can repeat. Log row
  intervals are not control-loop periods; filtering by state can join separate
  measurement segments. Control profile `owner_iteration` measures execution
  duration including preemption, not a start-to-start control period. These
  quantities must be compared with like-for-like firmware and experiment modes.
- Sequence gaps indicate missing saved sequence numbers, which may reflect
  intentional logging decimation and cannot independently identify capture loss.
  Overflow and truncated metadata must be checked when interpreting rates.

Example baseline on run305: all saved frames 400, median saved frame interval
100.0 ms, frame-to-delivery 50.187 ms, processing 12.359 ms, optimistic
frame-to-processed 62.681 ms, left/right valid 391/400 and 394/400. Actual
control-observed timestamps are absent in this existing log. These are logged
measurements, not predictions of fast mode performance.
