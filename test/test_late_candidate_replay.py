import unittest
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from late_candidate_replay import threshold, should_fire


class GeometryEnvelopeTests(unittest.TestCase):
    def test_conservative_threshold_in_both_directions(self):
        self.assertEqual(threshold(5.0, [1., 1.5, 2.]), 1.)
        self.assertEqual(threshold(-5.0, [-1., -1.5, -2.]), -1.)

    def test_already_passed_can_be_considered_but_is_not_contact(self):
        self.assertEqual(threshold(0.8, [1., 1.5, 2.]), 1.)

    def test_uncertain_models_are_rejected(self):
        self.assertIsNone(threshold(2., [-1., 1.]))
        self.assertIsNone(threshold(2., [1., None]))
        self.assertIsNone(threshold(2., [.2, 1.]))
        self.assertIsNone(threshold(2., [1., 4.]))
        self.assertIsNone(threshold(0., [1., 2.]))
        self.assertIsNone(threshold(float('nan'), [1., 2.]))
        self.assertIsNone(threshold(2., [1., float('nan')]))

    def test_deadline_and_direction_guard(self):
        self.assertTrue(should_fire(0.8, -10, 1, 1, False))
        self.assertFalse(should_fire(0, -10, 1, 1, False))
        self.assertFalse(should_fire(-.1, -10, 1, 1, False))
        self.assertFalse(should_fire(.8, 10, 1, 1, False))
        self.assertFalse(should_fire(.8, -10, 1, 1, True))
        self.assertFalse(should_fire(.8, float('nan'), 1, 1, False))


if __name__ == '__main__': unittest.main()
