"""Retrospectively exercise the bounded detector on recorded IMU rows.

This is NOT an outcome simulation: recorded motion was produced by the old
controller, and the experiment has not been run on hardware. Rows can be
decimated and timestamps rounded; early pulses cannot be recovered from them.
"""
import csv
import subprocess
import sys
import tempfile
from pathlib import Path
root = Path(__file__).resolve().parents[1]
folder = Path(sys.argv[1])
with (folder/'energy_control_autonomous_zero_cross_events.csv').open(newline='',encoding='utf8') as f:
    events = [r for r in csv.DictReader(f) if r.get('event_kind') == 'inward_stall_rescue' and r.get('output_executed') == 'True']
with (folder/'timeseries.csv').open(newline='',encoding='utf8') as f:
    rows = list(csv.DictReader(f))
with tempfile.TemporaryDirectory() as temp:
    exe = Path(temp)/('replay.exe' if sys.platform == 'win32' else 'replay')
    subprocess.run(['g++','-std=c++11','-O2',str(root/'tools/replay_rescue_rebound.cpp'),'-o',str(exe)],check=True)
    for event in events:
        start=int(event['zero_cross_time_ms']); values=[]
        for row in rows:
            try:
                t=round(float(row['time_s'])*1000)
                a=float(row['pitch_mekf_detector_relative_deg']);m=float(row['pitch_mekf_measurement_relative_deg'])
                v=float(row['physical_roll_rate_dps'])
            except (KeyError,ValueError,TypeError): continue
            if row['state_id']=='3' and start<=t<=start+1500:
                values.append(f'{t} {a} {m} {v} 1\n')
        result=subprocess.run([str(exe),str(start)],input=''.join(values),text=True,capture_output=True,check=True)
        print(f'rescue_ms={start} rows={len(values)} candidate_peak_ms_angle={result.stdout.strip() or "none"}')
