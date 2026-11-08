#!/usr/bin/env python3
"""Host/source identity contract for the second-stall experiment.

This checks the compiled macro and source data paths, not a live WebUI or a
decoded device RWLOG. The ESP ELF marker is checked separately by Pages CI.
"""
import subprocess
import sys
import tempfile
from pathlib import Path

root = Path(__file__).resolve().parents[1]
if len(sys.argv) != 2:
    raise SystemExit('usage: python tools/test_experiment_identity.py <expected-id>')
expected = sys.argv[1]
if expected != '1.0.100-second-stall-trial':
    raise SystemExit('unexpected experiment ID')
web = (root / 'src/web_ui.cpp').read_text(encoding='utf-8')
logger = (root / 'src/psram_logger.cpp').read_text(encoding='utf-8')
if '"{\\"revision\\":\\"" RUNTIME_VERSION' not in web:
    raise SystemExit('Web status does not include RUNTIME_VERSION')
if '"\\"firmware_revision\\":\\"" RUNTIME_VERSION' not in logger:
    raise SystemExit('RWLOG metadata does not include RUNTIME_VERSION')
with tempfile.TemporaryDirectory() as tmp:
    source = Path(tmp) / 'identity.cpp'
    exe = Path(tmp) / ('identity.exe' if sys.platform == 'win32' else 'identity')
    source.write_text('#include "runtime_diagnostics.h"\n#include <cstdio>\nint main(){std::puts(RUNTIME_VERSION);}\n', encoding='utf-8')
    def check(flags, value):
        subprocess.run(['g++', '-std=c++11', '-Wall', '-Wextra', '-Werror',
                        '-I' + str(root / 'src'), *flags, str(source), '-o', str(exe)], check=True)
        got = subprocess.check_output([str(exe)], text=True).strip()
        if got != value: raise SystemExit(f'identity {got!r}, expected {value!r}')
    check([], '1.0.99-alternate-foot-trial')
    check(['-DRESEARCH_RESCUE_REBOUND=1', '-DRESEARCH_FOLLOWUP_RESCUE=1'], expected)
print('Default and experiment version macro, UI source and RWLOG source identity PASS')
