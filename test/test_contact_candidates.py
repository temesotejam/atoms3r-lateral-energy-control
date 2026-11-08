import unittest
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from replay_contact_candidates import robust_target


class GuardTests(unittest.TestCase):
    def test_positive_and_negative(self):
        self.assertEqual(robust_target(5, [1, 1.5, 2]), (1, 'accepted'))
        self.assertEqual(robust_target(-5, [-1, -1.5, -2]), (-1, 'accepted'))

    def test_rejects_inconsistent_or_unavailable_model(self):
        self.assertEqual(robust_target(5, [1, -1])[1], 'sign_or_margin')
        self.assertEqual(robust_target(5, [None, 1])[1], 'missing_root')
        self.assertEqual(robust_target(5, [0.2, 1])[1], 'sign_or_margin')

    def test_rejects_passed_or_wide_thresholds(self):
        self.assertEqual(robust_target(2, [1, 2])[1], 'threshold_already_passed')
        self.assertEqual(robust_target(5, [1, 3])[1], 'model_spread')


if __name__ == '__main__':
    unittest.main()
