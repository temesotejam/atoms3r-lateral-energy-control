import csv
import json
from pathlib import Path
import tempfile
import unittest
from camera_metrics import num, stats, camera_subset, summarize

class CameraMetricTests(unittest.TestCase):
    def test_percentile_and_unknown(self):
        self.assertEqual(stats([])['median'],None)
        self.assertEqual(stats([10.,20.,30.])['p90'],28.)
        self.assertIsNone(num('nan'))

    def test_latency_and_missing_sequence_are_conditional(self):
        rows=[{'sequence':'1','timestamp_valid':'True','frame_valid':'True','frame_us':'1000','delivered_us':'3000','processing_us':'500','control_observed_us':'4000','left_valid':'True','right_valid':'False'},
              {'sequence':'3','timestamp_valid':'True','frame_valid':'True','frame_us':'101000','delivered_us':'103000','processing_us':'500','control_observed_us':'','left_valid':'','right_valid':'True'}]
        m=camera_subset(rows)
        self.assertEqual(m['recorded_sequence_missing_count'],1)
        self.assertEqual(m['recorded_frame_interval_ms']['median'],100)
        self.assertEqual(m['frame_to_processing_end_lower_bound_ms']['median'],2.5)
        self.assertEqual(m['frame_to_control_observed_ms']['median'],3)
        self.assertEqual(m['left_valid_rate_recorded']['known'],1)
        self.assertEqual(m['right_valid_rate_recorded']['percent'],50)

    def test_fast_aggregate_and_old_missing_optional_files(self):
        with tempfile.TemporaryDirectory() as d:
            p=Path(d)
            p.joinpath('metadata.json').write_text(json.dumps({'foot_observation':{'fast_run_frames':11,'fast_run_first_us':1000,'fast_run_last_us':1001000,'fast_run_received':12,'fast_run_invalid_frame':2,'fast_run_right_invalid':3,'fast_run_left_invalid':4}}),encoding='utf-8')
            m=summarize(p)
            self.assertEqual(m['camera_metadata']['fast_run_mean_fps_from_metadata'],10)
            self.assertEqual(m['camera_metadata']['fast_frame_valid_rate_percent'],83.33)
            self.assertEqual(m['camera_metadata']['fast_right_marker_valid_rate_percent'],75)
            self.assertEqual(m['camera_metadata']['fast_left_marker_valid_rate_percent'],66.67)
            self.assertIsNone(m['imu_measurement_state_3']['imu_update_dt_ms']['median'])
            self.assertEqual(m['camera_recorded_all_states']['recorded_rows'],0)

if __name__=='__main__': unittest.main()
