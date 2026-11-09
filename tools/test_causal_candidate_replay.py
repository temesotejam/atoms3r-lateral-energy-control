import math
from pathlib import Path
import unittest

from causal_candidate_replay import frame_pose, run
from batch_foot_timing import interpolate


class CausalPoseTest(unittest.TestCase):
    def test_frame_pose_never_uses_future_state(self):
        imu = [(100., 0., 0., 1.), (120., 0., 0., 2.), (140., 0., 0., 3.)]
        times = [v[0] for v in imu]
        self.assertEqual(frame_pose(imu, times, 130., 160.), 2.)
        self.assertEqual(frame_pose(imu, times, 130., 110.), None)
        self.assertIsNone(frame_pose(imu, times, 170., 200.))
        self.assertIsNone(frame_pose(imu, times, 90., 200.))

    def test_calibration_interpolation_and_range(self):
        self.assertAlmostEqual(interpolate([[10,20],[0,0]], 2.5), 5)
        with self.assertRaises(ValueError): interpolate([[0,0],[10,20]], -1)

    def test_invalid_extra_delay_rejected_before_reading_files(self):
        for x in (-1, math.nan, math.inf):
            with self.assertRaises(ValueError): run(Path('missing'),0,0,extra_delay_ms=x)


if __name__ == '__main__': unittest.main()
