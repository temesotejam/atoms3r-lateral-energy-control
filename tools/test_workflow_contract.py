"""Check draft workflow isolation and its deliberate fail-closed placeholders."""
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]
def workflow():
    draft = ROOT / '.github/workflows/pages.template.yml'
    return (draft if draft.exists() else ROOT / '.github/workflows/pages.yml').read_text(encoding='utf-8')


class WorkflowContract(unittest.TestCase):
    def test_template_will_not_build_unconfigured_experiment(self):
        s = workflow()
        match = re.search(r'EXPERIMENT_COMMIT: "([^"]+)"', s)
        self.assertIsNotNone(match)
        self.assertTrue(match.group(1) == '__REPLACE_WITH_40_HEX_COMMIT__' or
                        re.fullmatch(r'[0-9a-f]{40}', match.group(1)))
        self.assertIn('EXPERIMENT_ID: "1.0.100-second-stall-trial"', s)
        self.assertIn('EXPERIMENT_BUILD_FLAG_1: "-DRESEARCH_RESCUE_REBOUND=1"', s)
        self.assertIn('EXPERIMENT_BUILD_FLAG_2: "-DRESEARCH_FOLLOWUP_RESCUE=1"', s)
        self.assertIn('"$EXPERIMENT_BUILD_FLAG_1" "$EXPERIMENT_BUILD_FLAG_2"', s)
        self.assertIn('[[ "$EXPERIMENT_COMMIT" =~ ^[0-9a-f]{40}$ ]]', s)
        self.assertIn('test "$(git rev-parse HEAD)" = "$EXPERIMENT_COMMIT"', s)
        self.assertIn('python3 tools/test_experiment_identity.py "$EXPERIMENT_ID"', s)
        self.assertIn('xtensa-esp32s3-elf-strings', s)

    def test_distinct_artifact_and_exact_layout_check(self):
        s = workflow()
        self.assertIn('name: walking-experiment-firmware', s)
        self.assertIn('path: _site/experiment/firmware', s)
        self.assertIn('--stable-firmware _site/firmware', s)
        self.assertIn('path: _site/camera', s)
        self.assertIn('cp -R site/. _site/', s)
        self.assertIn('cp .pio/build/atoms3cam/firmware.bin _site/firmware/firmware-freefoot-runtime-v2.bin', s)
        self.assertIn('needs: [pages-ready, build-camera-diagnostics, build-walking-experiment]', s)

    def test_pr_dry_run_cannot_deploy(self):
        s = workflow()
        self.assertIn('pull_request:', s)
        deploy = s[s.index('\n  deploy:'):]
        self.assertIn("if: github.event_name != 'pull_request'", deploy)
        self.assertIn('Verify published experiment', deploy)


if __name__ == '__main__': unittest.main()
