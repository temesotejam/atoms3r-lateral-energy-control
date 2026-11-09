import hashlib
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
import zipfile

SCRIPT = Path(__file__).with_name('prepare_camera_compare_package.py')


class CameraPackageTests(unittest.TestCase):
    def test_each_mode_manifest_hashes_and_zip(self):
        for mode in range(3):
            with self.subTest(mode=mode), tempfile.TemporaryDirectory() as tmp:
                root = Path(tmp)
                (root / 'firmware').mkdir()
                for name in ('bootloader.bin', 'partitions.bin', 'boot_app0.bin', 'firmware.bin'):
                    (root / 'firmware' / name).write_bytes(name.encode())
                sha = 'a' * 40
                subprocess.run([sys.executable, str(SCRIPT), '--mode', str(mode),
                                '--commit', sha, '--checkout-commit', 'b' * 40,
                                '--output', str(root)], check=True)
                manifest = json.loads((root / 'manifest.json').read_text(encoding='utf-8'))
                self.assertIn(f'mode-{mode}-{sha[:12]}', manifest['version'])
                parts = manifest['builds'][0]['parts']
                self.assertEqual([p['offset'] for p in parts], [0, 32768, 57344, 65536])
                lines = (root / 'SHA256SUMS.txt').read_text().splitlines()
                for part, line in zip(parts, lines):
                    data = (root / part['path']).read_bytes()
                    self.assertEqual(line, hashlib.sha256(data).hexdigest() + '  ' + part['path'])
                page = (root / 'index.html').read_text(encoding='utf-8')
                self.assertIn(f'Mode {mode}', page)
                self.assertIn(sha, page)
                self.assertIn('manifest="manifest.json"', page)
                self.assertIn('静置診断専用', page)
                self.assertIn('非ゼロ電流指令を禁止', page)
                self.assertIn('実機未検証', page)
                readme = (root / 'README.txt').read_text(encoding='utf-8')
                self.assertIn('静置診断専用', readme)
                for text in (page, readme):
                    self.assertIn('60秒', text)
                    self.assertIn('30秒', text)
                    self.assertIn('ログ転送', text)
                with zipfile.ZipFile(root / 'package.zip', 'w') as archive:
                    for p in root.rglob('*'):
                        if p.is_file() and p.name != 'package.zip':
                            archive.write(p, p.relative_to(root))
                with zipfile.ZipFile(root / 'package.zip') as archive:
                    self.assertIsNone(archive.testzip())
                    self.assertEqual(json.loads(archive.read('manifest.json')), manifest)
                    self.assertTrue(all(archive.read(p['path']) for p in parts))

    def test_invalid_or_missing_image_is_rejected(self):
        with tempfile.TemporaryDirectory() as tmp:
            result = subprocess.run([sys.executable, str(SCRIPT), '--mode', '0',
                                     '--commit', 'a' * 40, '--checkout-commit', 'b' * 40,
                                     '--output', tmp], capture_output=True)
            self.assertNotEqual(result.returncode, 0)


if __name__ == '__main__':
    unittest.main()
