import hashlib
import json
import tempfile
import unittest
from pathlib import Path
import package_experiment_web as pkg
import verify_experiment_web as verifier

COMMIT = '0123456789abcdef' * 2 + '01234567'
EXPERIMENT = '1.0.100-second-stall-trial'
ROOT = Path(__file__).resolve().parents[1]


class ExperimentWebTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name) / 'site' / 'experiment'
        self.stable = Path(self.temp.name) / 'stable'
        (self.root / 'firmware').mkdir(parents=True)
        self.stable.mkdir()
        for name in pkg.PARTS:
            b = (name + '-test').encode()
            (self.root / 'firmware' / name).write_bytes(b)
            (self.stable / name).write_bytes(b)
        (self.root / 'index.html').write_bytes((ROOT / 'site/experiment/index.html').read_bytes())

    def tearDown(self): self.temp.cleanup()

    def pack(self): pkg.package(self.root, self.stable, COMMIT, EXPERIMENT)
    def verify(self): verifier.verify(lambda n: (self.root / n).read_bytes(), EXPERIMENT, COMMIT)
    def revision(self, commit=COMMIT, data=b'firmware.bin-test'):
        return self.root / 'revisions' / commit / hashlib.sha256(data).hexdigest()

    def test_valid(self):
        self.pack(); self.verify()
        m = json.loads((self.revision() / 'manifest.json').read_text())
        self.assertTrue(m['new_install_prompt_erase'])
        self.assertEqual((self.revision() / 'firmware/firmware.bin').read_bytes(), b'firmware.bin-test')

    def test_shared_firmware_mismatch_rejected_before_manifest(self):
        (self.root / 'firmware/partitions.bin').write_bytes(b'different')
        with self.assertRaises(ValueError): self.pack()
        self.assertFalse((self.root / 'BUILD.txt').exists())

    def test_oversized_partition_cannot_reach_nvs(self):
        (self.root / 'firmware/partitions.bin').write_bytes(b'x' * 0x1001)
        with self.assertRaises(ValueError): self.pack()

    def test_tampered_firmware_rejected(self):
        self.pack(); (self.revision() / 'firmware/firmware.bin').write_bytes(b'different')
        with self.assertRaises(ValueError): self.verify()

    def test_each_revision_has_own_binary_path(self):
        self.pack()
        other = 'f' * 40
        (self.root / 'firmware/firmware.bin').write_bytes(b'new-build')
        pkg.package(self.root, self.stable, other, EXPERIMENT)
        self.assertEqual((self.revision() / 'firmware/firmware.bin').read_bytes(), b'firmware.bin-test')
        self.assertEqual((self.revision(other, b'new-build') / 'firmware/firmware.bin').read_bytes(), b'new-build')
        with self.assertRaises(ValueError): self.verify()  # BUILD now identifies the other revision.

    def test_same_commit_different_binary_has_different_path(self):
        self.pack()
        (self.root / 'firmware/firmware.bin').write_bytes(b'other-binary')
        self.pack()
        self.assertEqual((self.revision() / 'firmware/firmware.bin').read_bytes(), b'firmware.bin-test')
        self.assertEqual((self.revision(COMMIT, b'other-binary') / 'firmware/firmware.bin').read_bytes(), b'other-binary')
        self.verify()  # BUILD points to the second verified payload.

    def test_placeholder_and_bad_identifier_fail_closed(self):
        with self.assertRaises(ValueError): pkg.package(self.root, self.stable, '__COMMIT__', EXPERIMENT)
        with self.assertRaises(ValueError): pkg.package(self.root, self.stable, COMMIT, '../unstable')
        with self.assertRaises(ValueError): pkg.package(self.root, self.stable, COMMIT, 'v1..other')

    def test_erase_fallback_missing_rejected(self):
        self.pack(); p = self.revision() / 'manifest.json'; m = json.loads(p.read_text())
        m['new_install_prompt_erase'] = False
        p.write_text(json.dumps(m))
        with self.assertRaises(ValueError): self.verify()

    def test_patch_must_load_before_module(self):
        self.pack(); p = self.root / 'index.html'; p.write_text('esp-web-tools@10.1.1 ../flasher-completion.js Erase device manifest="manifest.json" id="flash"')
        with self.assertRaises(ValueError): self.verify()


if __name__ == '__main__': unittest.main()
