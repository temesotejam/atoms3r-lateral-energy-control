#!/usr/bin/env python3
"""Stage one identified experimental ESP32-S3 build inside the existing site.

The physical device and NVS retention are not tested by this packager.
Run only after the exact experimental source and build flags are finalized.
"""
import argparse
import hashlib
import json
import re
from pathlib import Path

PARTS = {
    'bootloader.bin': (0x0000, 0x8000),
    'partitions.bin': (0x8000, 0x1000),
    'boot_app0.bin': (0xe000, 0x2000),
    'firmware.bin': (0x10000, 0x330000),
}
COMMON = ('bootloader.bin', 'partitions.bin', 'boot_app0.bin')


def package(root: Path, stable: Path, commit: str, experiment_id: str):
    if not re.fullmatch(r'[0-9a-f]{40}', commit):
        raise ValueError('A fixed 40-digit source SHA is required')
    if not re.fullmatch(r'[a-z0-9][a-z0-9.-]{2,63}', experiment_id) or '..' in experiment_id:
        raise ValueError('A unique lowercase experiment ID is required')
    files = {}
    contents = {}
    for name, (_, limit) in PARTS.items():
        p = root / 'firmware' / name
        data = p.read_bytes()
        if not data or len(data) > limit:
            raise ValueError(f'invalid flash size: {name} ({len(data)} bytes)')
        if name in COMMON and data != (stable / name).read_bytes():
            raise ValueError(f'experimental and stable {name} differ')
        files[name] = hashlib.sha256(data).hexdigest()
        contents[name] = data
    manifest = {
        'name': 'AtomS3R walking experiment: ' + experiment_id,
        'version': 'experiment-' + experiment_id + '-' + commit[:12],
        # ESP Web Tools 10.1.1 otherwise defaults to full-device erase when
        # no Improv client is found. Fallback remains unchecked by default.
        'new_install_prompt_erase': True,
        'new_install_improv_wait_time': 0,
        'builds': [{'chipFamily': 'ESP32-S3', 'parts': [
            {'path': 'firmware/' + name, 'offset': offset}
            for name, (offset, _) in PARTS.items()]}],
    }
    # Both source revision and actual binary content hash are in every URL.
    # Identical source can otherwise build differently after dependency changes.
    # Publish BUILD.txt only after every versioned file has been written.
    revision = root / 'revisions' / commit / files['firmware.bin']
    (revision / 'firmware').mkdir(parents=True, exist_ok=False)
    for name, data in contents.items():
        (revision / 'firmware' / name).write_bytes(data)
    (revision / 'SHA256SUMS.txt').write_bytes(''.join(f'{h}  firmware/{n}\n' for n, h in files.items()).encode('utf-8'))
    (revision / 'manifest.json').write_bytes((json.dumps(manifest, indent=2) + '\n').encode('utf-8'))
    (root / 'BUILD.txt').write_bytes(f'experiment_id={experiment_id}\ncommit={commit}\nfirmware_sha256={files["firmware.bin"]}\n'.encode('utf-8'))


def main():
    p = argparse.ArgumentParser()
    p.add_argument('--root', type=Path, required=True)
    p.add_argument('--stable-firmware', type=Path, required=True)
    p.add_argument('--commit', required=True)
    p.add_argument('--experiment-id', required=True)
    args = p.parse_args()
    package(args.root, args.stable_firmware, args.commit, args.experiment_id)


if __name__ == '__main__':
    main()
