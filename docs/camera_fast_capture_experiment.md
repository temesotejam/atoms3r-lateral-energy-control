# Camera pacing and buffer lifetime experiment

Research copy only. The default `FOOT_FAST_CAPTURE_EXPERIMENT=0` retains the normal capture path and log behavior.

* 1: remove the minimum 66.667 ms observer-loop pacing; keep the camera buffer until processing finishes.
* 2: allocate a 76800-byte PSRAM frame copy, copy each valid grayscale frame immediately after delivery, return the camera buffer, run both existing marker trackers and idle preview on the private copy, and remove pacing. Allocation failure falls back to mode 0.

Modes 1 and 2 are experimental build flags, not UI settings, and must not be released without on-device tests. Modes change the frequency of the existing live foot-adaptation observations; all log controls and motor safety gates must be evaluated separately. Mode 2 adds PSRAM allocation/bandwidth. Actual benefit is unknown: with a single camera buffer and sensor VSYNC timing, returning it immediately after acquisition may still miss the next frame.

Foot log rows are gated to 15 Hz average in experimental modes to preserve the existing 768-row capacity for a 40-second run. All frames are still offered to the live control observer; `fast_record_skipped` tracks the difference. Logged rows can have sequence gaps by design. Metadata includes `fast_run_frames`, first and last valid driver timestamps, `fast_copy_max_us`, pacing mode and `fast_capture_active`; `processing_us` includes frame-copy duration. Compare mode 0/1/2 frame period, camera age, marker failure rate, control/IMU timing and frame-log integrity in matched trials. Driver timestamps are not independently verified exposure timestamps. Camera-observation timestamp instrumentation that only attaches to a saved row cannot record the times of deliberately unlogged frames.

## Local checks

The standalone `test/test_foot_fast_record_gate.cpp` checks 40s simulated 20Hz and 10Hz streams fit capacity, plus invalid timestamps and a dropped-frame gap. Compile `src/foot_observer.cpp` with `tools/host_v46o` and each flag value 0, 1 and 2 to check the guarded source. These are not firmware, device performance, RAM fragmentation, or camera concurrency tests.
