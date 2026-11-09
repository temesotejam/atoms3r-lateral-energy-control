# Foot geometry early-timing retrospective: multi-run sensitivity

Research only; does not command the motor, establish support transfer, or validate
the 3D transforms. Run from the repository root after converting each RWLOG:

```
python tools/batch_foot_timing.py path/to/run305 path/to/run337 path/to/run329 path/to/run404
```

Each run uses its own per-boot pixel and piecewise calibration table to estimate
its nominal offsets. Those offsets assume a correct boot pose and unchanged
mounting. Three scenarios add 0/20/50 ms to the optimistic frame availability
bound; they are sensitivity experiments, not measured publication delays. This
replay mixes latest sampled IMU pitch and older camera frames and does not
establish synchronized 3D pose. Output event matching uses recorded future ZERO
only for scoring and uses no ground-truth contact labels. A diagnostic count
excludes candidates within 250 ms of the previous recorded ZERO. This is not a
simulation of the modified controller: an early pulse changes the trajectory,
and motor, peak and safety state are not reconstructed.

For logs 30581450, 33762516, 32906448, 40418357 respectively, screened
matches with +0 ms were 21/34, 35/61, 34/81, 34/52; with +50 ms they were
21/34, 33/61, 26/81, 28/52. These denominators include startup and target
stages; they are not rates of successful early actuation or foot transfer.
For run305, only two initial candidates follow a logged 8-degree stage peak;
the rest occur during staged ramp-up. Contact, actual control-observed frame
time, camera/body synchronization and mechanical calibration remain unknown.

For real hardware, first measure control-observed frame timing with matching
sequence IDs; evaluate pose at the frame time and reject stale or ambiguous
states. A potential early-input controller needs existing motor and minimum
half-cycle gates, no duplicate pulse, and ordinary ZERO reevaluation on a
definite no-send result. Hardware communication/faults preclude guaranteeing
physical current by ZERO. No experimental early motor command is enabled here.
