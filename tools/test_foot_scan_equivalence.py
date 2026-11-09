"""Regression-check the row-major foot scan against the frozen pre-change scan.

Compiles both implementations and compares every reported observation field
except processing time on deterministic scenes. Host timings are informational
and cannot be used as ESP32/PSRAM performance measurements.
"""
import hashlib
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
source = (ROOT / 'src/white_marker_tracker.cpp').read_text(encoding='utf-8')
start = source.index('    // Visit each scan row contiguously:')
end = source.index('    int rolling = 0;', start)
baseline = '''    for (int x = 0; x < kWidth; ++x) {
        int a = 0, b = 0;
        for (int i = 0; i < appcfg::kWhiteMarkerRowCount; ++i) a += gray[(rows[i] + dy) * kWidth + x];
        for (int i = 0; i < appcfg::kWhiteReferenceRowCount; ++i) b += gray[(refs[i] + dy) * kWidth + x];
        contrast[x] = 3 * a - 5 * b;
    }
'''
reference = source[:start] + baseline + source[end:]
with tempfile.TemporaryDirectory() as tmp:
    tmp = Path(tmp)
    old = tmp / 'white_marker_tracker_reference.cpp'
    old.write_text(reference, encoding='utf-8')
    outputs = []
    for name, tracker in [('reference', old), ('optimized', ROOT / 'src/white_marker_tracker.cpp')]:
        exe = tmp / (name + ('.exe' if os.name == 'nt' else ''))
        subprocess.run(['g++', '-std=c++17', '-O2', '-Wall', '-Wextra', '-Werror',
                        '-I' + str(ROOT / 'tools/host_v46o'), '-I' + str(ROOT / 'src'),
                        str(ROOT / 'tools/benchmark_vision.cpp'), str(tracker), '-o', str(exe)], check=True)
        run = subprocess.run([str(exe)], check=True, capture_output=True)
        outputs.append(run.stdout)
        print(name + ' host only: ' + run.stderr.decode().strip())
    if outputs[0] != outputs[1]:
        a, b = outputs[0].splitlines(), outputs[1].splitlines()
        first = next((i for i, (x, y) in enumerate(zip(a, b)) if x != y), None)
        raise AssertionError(f'detector output changed at row {first}: {len(a)} vs {len(b)}')
    print('2000 deterministic frames, both tracks, exact output match:', hashlib.sha256(outputs[0]).hexdigest())
    print('Includes black/white full frames, clipping, null frames, noise, stable and changing tracks.')
