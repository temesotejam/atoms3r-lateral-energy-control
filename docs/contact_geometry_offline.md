# Offline foot geometry and camera timing experiment

This is an offline, hypothesis-only tool. It does not change controller output,
detect ground contact, or validate a motor trigger. The 1.0.99 firmware is unchanged.

The split circular sole has a 150 mm outer radius, x ranges [-45,-5] and [5,45]
mm, and a 56 mm fore/aft extent. These are the reference geometry documented at
https://github.com/temesotejam/rw-walker-digital-twin/blob/main/docs/REFERENCE_GEOMETRY.md.
Axes are x lateral, y fore/aft, z up. Each leg is *assumed* to rotate about an
x-oriented hip hinge at 120 mm height, not a verified current-assembly joint.
An independent inspection of the local 2026-11-04 `Assembly 1 (1).step`
found X-axis hole candidates corresponding to nominal heights near 82.83 mm
and 55.33 mm from the sole inner edge. The particular active hip axis and
current assembly state have not been verified, so neither is a confirmed hip
height. Users can pass candidate heights to `switch` for sensitivity checks.
Both feet are assumed rigid, independent and connected to the same body. The
minimum height of each full sole surface is computed in a common body pose and
the root of their height difference is found. Equal minimum heights are a
geometric compatibility condition, not a force-based support transfer,
touchdown time, or dynamic/no-slip simulation. Sole endpoints, finite width,
body pitch, independent leg angles and central gap matter to the result.

The camera-to-joint transform, per-foot signs and body-pitch sign are not
fully identified here. The calibration intentionally held the foot fixed and
rotated the camera-bearing body through an equivalent relative angle; under a
rigid, single-axis model this is a useful calibration of body-to-foot pose.
Actual signs, boot reference, off-axis perspective and dynamic accuracy are
not yet independently verified. The replay treats logged `left_deg` and
`right_deg` as tentative joint-angle magnitudes and enumerates signs and body
pitch sign. Its candidate envelope is a sensitivity study, not a confidence
interval or a proven physical prediction. The replay also reports each
per-boot zero's angle relative to the piecewise table's nominal zero; neither
reference is independently verified as the mechanical centre of the leg.

Usage, from the repository root after converting an RWLOG to CSV:

```
python tools/replay_contact_uncertainty.py path/to/csv_directory
python tools/replay_zero_prediction.py path/to/csv_directory
python test/test_contact_geometry_probe.py
```

The geometry sensitivity replay aligns observations retrospectively to 80 ms
before each recorded zero event. It is useful for investigating hypotheses,
but choosing that time with a future recorded event is not an online predictor
and is not evidence of predicting touchdown. The independent zero-prediction
replay samples the IMU at each frame's estimated availability, makes a linear
constant-rate prediction from the then-available state, and compares it later
with the recorded crossing. It does not estimate contact or motor response.
Both scripts include marker processing time after frame delivery.
Frame timestamp versus true exposure and
additional publish/scheduling latency remain unvalidated; this is an optimistic
availability bound, not proof the control task had the data. A physical input implementation would need a
causal prediction of the future pose, a validated mapping and signed coordinate
frames, a check for actual intervention success, an explicit pre-centre attempt,
and a normal-zero fallback if no earlier command was sent. Motor/IMU faults and
existing safety gates can still prevent a physical input.

## Calibration and timing follow-up

The existing foot calibration notes describe the positive calibration poses as
forward foot angles, and the table zero as the upright-leg reference. With
unchanged mounting, adding the table angle at each boot-zero pixel to the
logged relative angle estimates that reference; it does not distinguish actual
boot pose from mounting drift. In run305 those corrections are about +1.246 deg
left and +1.784 deg right. This does not by itself validate a three-dimensional
joint transform or establish actual touchdown.

`delivered_us + processing_us` is an optimistic lower bound: camera-buffer
release, publication and up to a control polling interval are not recorded in
the existing frame. Offline candidate timing therefore cannot be proven causal
for the controller until the actual control-observed timestamp is measured.

The research branch also prototypes `control_observed_us` for a recorded foot
frame: this is captured immediately after the control task reads the snapshot.
A matching recorded run and sequence are required; unmatched frames are null.
The sparse table retains up to the first 64 matching frames, subject to metadata
headroom, and reports truncation; later observations are unavailable.
It is diagnostic only, not a contact detector, and adds a critical-section
operation and PSRAM per frame. Firmware build and on-device load/timing tests
must pass before deployment. Existing logs do not contain this timestamp.
