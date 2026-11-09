"""Replay completed logged pairs only. Detection describes, not causes, period 2."""
import csv
import subprocess
import sys
import tempfile
from pathlib import Path

root=Path(__file__).resolve().parent
folder=Path(sys.argv[1])
with (folder/'energy_control_autonomous_peak_events.csv').open(newline='',encoding='utf8') as f:
    peaks=list(csv.DictReader(f))
with (folder/'energy_control_autonomous_zero_cross_events.csv').open(newline='',encoding='utf8') as f:
    events=list(csv.DictReader(f))

def outcome(e):
    if e.get('valid')!='True' or e.get('event_kind')!='energy_control_zero_cross': return 0
    if e.get('output_executed')=='True' and int(e.get('pulse_width_ms') or 0)>0:return 1
    if e.get('output_executed')=='False' and int(e.get('pulse_width_ms') or 0)==0 and e.get('reason')=='VALID_NO_OUTPUT' and e.get('work_control_branch')=='15':return 2
    return 0

def decision(prev,peak):
    if peak.get('pending_command_matched')!='True':return None
    try:lo=int(prev['peak_time_ms']); hi=int(peak['peak_time_ms']); side=int(peak['physical_peak_side'])
    except (ValueError,KeyError): return None
    matching=[e for e in events if e.get('physical_next_peak_side')==str(side) and e.get('zero_cross_time_ms') and lo<int(e['zero_cross_time_ms'])<hi and outcome(e)]
    return matching[-1] if matching else None

lines=[]
completed=0
for i in range(1,len(peaks)-1):
    prev,strong,weak=peaks[i-1:i+2]
    try:
        side=int(strong['physical_peak_side'])
        if side not in (-1,1) or int(weak['physical_peak_side'])!=-side:
            lines.append('0 0 0 0 0 0 0 0 0 0 0\n');continue
        if abs(float(strong['target_peak_deg'])-float(weak['target_peak_deg']))>0.05:
            lines.append(f'{side} 0 0 0 0 0 0 0 0 0 0\n');continue
        first,second=decision(prev,strong),decision(strong,weak)
        if not first or not second:
            lines.append(f'{side} 0 0 0 0 0 0 0 0 0 0\n');continue
        sp=int(strong['peak_time_ms']);wp=int(weak['peak_time_ms'])
        completed+=1
        lines.append(f"{side} {wp} {first['zero_cross_time_ms']} {sp} {second['zero_cross_time_ms']} {wp} {strong['target_peak_deg']} {strong['peak_amplitude_deg']} {weak['peak_amplitude_deg']} {outcome(first)} {outcome(second)}\n")
    except (ValueError,KeyError,TypeError):lines.append('0 0 0 0 0 0 0 0 0 0 0\n')
with tempfile.TemporaryDirectory() as d:
    exe=Path(d)/('replay.exe' if sys.platform=='win32' else 'replay')
    subprocess.run(['g++','-std=c++11','-O2',str(root/'replay_period2.cpp'),'-o',str(exe)],check=True)
    r=subprocess.run([str(exe)],input=''.join(lines),text=True,capture_output=True,check=True)
    print(f'{folder}: completed_pairs={completed} explicit_resets={len(lines)-completed} detections=\n{r.stdout}',end='')
