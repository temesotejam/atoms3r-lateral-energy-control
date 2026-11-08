# Offline foot geometry and camera timing experiment

This is an offline, hypothesis-only tool. It does not change controller output,
detect ground contact, or validate a motor trigger. The 1.0.99 firmware is unchanged.

The split circular sole has a 150 mm outer radius, x ranges [-45,-5] and [5,45]
mm, and a 56 mm fore/aft extent. These are the reference geometry documented at
https://github.com/temesotejam/rw-walker-digital-twin/blob/main/docs/REFERENCE_GEOMETRY.md.
Axes are x lateral, y fore/aft, z up. Each leg is *assumed* to rotate about an
x-oriented hip hinge at 120 mm height, not a verified current-assembly joint.
Both feet are assumed rigid, independent and connected to the same body. The
minimum height of each full sole surface is computed in a common body pose and
the root of their height difference is found. Equal minimum heights are a
geometric compatibility condition, not a force-based support transfer,
touchdown time, or dynamic/no-slip simulation. Sole endpoints, finite width,
body pitch, independent leg angles and central gap matter to the result.

The camera-to-joint transform, per-foot signs and body-pitch sign are not
identified here. The replay treats logged `left_deg` and `right_deg` as
hypothetical joint-angle magnitudes and enumerates their signs and body pitch
sign. Its candidate envelope is only a sensitivity study, not a confidence
interval and not a physical prediction. The foot calibration in the logger was
obtained with fixed legs and is not yet validated as a moving-joint angle.

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
