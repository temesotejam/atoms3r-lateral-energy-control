# Offline rescue rebound detector

This is a positive-side, offline research detector only. It is not connected to
the runner or to motor output. It must not be treated as a recovery controller.

Run `python tools/replay_rescue_rebound.py <decoded-rwlog-directory>` after
decoding the log, or compile and run `tools/test_rescue_rebound.cpp` with C++11.
The replay uses logged IMU samples and the recorded inward-stall-rescue time.
Its sampling is not the original realtime control loop, and future logged motion
reflects the old controller, not a counterfactual intervention.

In the 1.0.99 run305 the rescue at 13.866 s is followed by a positive-side
candidate around 14.370 s, 5.1 degrees. The logged trajectory then did not
cross zero before the recording ended. Therefore merely detecting/rearming at
that peak and waiting for zero would not have recovered that recorded run.

Zero acceptance, failed peak registration, duplicate pulses, sensor faults,
adaptive state resets, timing and motor response require full runner and device
tests. Existing offline detector tests cannot establish those properties.
