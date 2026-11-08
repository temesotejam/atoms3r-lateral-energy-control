#!/usr/bin/env python3
"""Verify the assembled or published diagnostic-independent experiment page."""
import argparse
import hashlib
import json
import re
import time
from pathlib import Path
from urllib.request import Request, urlopen

PARTS = {'firmware/bootloader.bin': (0x0000, 0x8000),
         'firmware/partitions.bin': (0x8000, 0x1000),
         'firmware/boot_app0.bin': (0xe000, 0x2000),
         'firmware/firmware.bin': (0x10000, 0x330000)}


def verify(read, experiment_id, commit):
    if (not re.fullmatch(r'[0-9a-f]{40}', commit) or
        not re.fullmatch(r'[a-z0-9][a-z0-9.-]{2,63}', experiment_id) or '..' in experiment_id):
        raise ValueError('unresolved experiment identity')
    html = read('index.html').decode('utf-8')
    if html.index('../flasher-completion.js') >= html.index('esp-web-tools@10.1.1'):
        raise ValueError('no-erase patch loaded too late')
    if ('Erase device' not in html or 'id="flash"' not in html or
        "setAttribute('manifest',manifestPath)" not in html or
        "'revisions/'+sha.slice(7)+'/'+firmwareHash+'/manifest.json'" not in html):
        raise ValueError('flasher UI is incomplete')
    identity = read('BUILD.txt').decode('utf-8').splitlines()
    if f'experiment_id={experiment_id}' not in identity or f'commit={commit}' not in identity:
        raise ValueError('published experiment identity mismatch')
    fw_lines = [x for x in identity if re.fullmatch(r'firmware_sha256=[0-9a-f]{64}', x)]
    if len(fw_lines) != 1:
        raise ValueError('missing firmware identity')
    firmware_hash = fw_lines[0].split('=')[1]
    prefix = 'revisions/' + commit + '/' + firmware_hash + '/'
    manifest = json.loads(read(prefix + 'manifest.json'))
    if (manifest.get('new_install_prompt_erase') is not True or
        manifest.get('new_install_improv_wait_time') != 0 or
        manifest.get('version') != f'experiment-{experiment_id}-{commit[:12]}'):
        raise ValueError('manifest safety or identity mismatch')
    builds = manifest.get('builds', [])
    if len(builds) != 1 or builds[0].get('chipFamily') != 'ESP32-S3':
        raise ValueError('chip family mismatch')
    parts = builds[0].get('parts', [])
    if len(parts) != 4 or {p['path']: p['offset'] for p in parts} != {k: v[0] for k, v in PARTS.items()}:
        raise ValueError('flash layout mismatch')
    hashes = {}
    for line in read(prefix + 'SHA256SUMS.txt').decode('utf-8').splitlines():
        digest, name = line.split(maxsplit=1)
        if name in hashes or name not in PARTS or not re.fullmatch('[0-9a-f]{64}', digest):
            raise ValueError('bad checksum entry')
        hashes[name] = digest
    if set(hashes) != set(PARTS) or hashes['firmware/firmware.bin'] != firmware_hash:
        raise ValueError('incomplete checksum list')
    for name, (_, limit) in PARTS.items():
        blob = read(prefix + name)
        if not blob or len(blob) > limit or hashlib.sha256(blob).hexdigest() != hashes[name]:
            raise ValueError('invalid flash binary: ' + name)
    print('Experiment identity, layout, erase fallback and binary checksums verified')


def main():
    p = argparse.ArgumentParser()
    group = p.add_mutually_exclusive_group(required=True)
    group.add_argument('--path', type=Path)
    group.add_argument('--url')
    p.add_argument('--experiment-id', required=True)
    p.add_argument('--commit', required=True)
    a = p.parse_args()
    if a.path:
        return verify(lambda n: (a.path / n).read_bytes(), a.experiment_id, a.commit)
    def fetch(name):
        req = Request(a.url.rstrip('/') + '/' + name + '?verify=' + a.commit,
                      headers={'Cache-Control': 'no-cache'})
        with urlopen(req, timeout=30) as resp:
            return resp.read()
    for n in range(8):
        try:
            return verify(fetch, a.experiment_id, a.commit)
        except Exception:
            if n == 7: raise
            time.sleep(10)


if __name__ == '__main__': main()
