"""Approximate replay of recorded body peaks and camera frame availability.

Arrival assumes delivery+processing+12.5ms; neither actual control polling
nor peak-detection processing latency is recorded. Use only for regression
comparison, never to claim exact device behavior.
"""
import csv
import subprocess
import sys
from pathlib import Path


def rows(path):
    with path.open(newline='', encoding='utf8') as f:
        return list(csv.DictReader(f))


def lines(directory):
    events=[]
    for r in rows(directory/'energy_control_autonomous_peak_events.csv'):
        if r['peak_time_ms'] and r['physical_peak_side'] in ('1','-1'):
            ms=int(r['peak_time_ms'])
            events.append((ms,0,f"P {ms} {r['physical_peak_side']}\n"))
    for r in rows(directory/'foot_angles.csv'):
        try:
            t=int(r['measurement_time_us'])//1000
            latency=(int(r['delivered_us'])-int(r['frame_us'])+int(r['processing_us']))/1000+12.5
            if t<0 or latency<0 or r['timestamp_valid']!='True': continue
            lv=r['left_valid']=='True'; rv=r['right_valid']=='True'
            left=float(r['left_deg']) if lv else 0.0
            right=float(r['right_deg']) if rv else 0.0
            events.append((t+latency,1,f'F {t} {int(lv)} {int(rv)} {left} {right}\n'))
        except (ValueError, KeyError, TypeError):
            continue
    return ''.join(x[2] for x in sorted(events))


def main():
    old,new=map(Path,sys.argv[1:3])
    for d in map(Path,sys.argv[3:]):
        payload=lines(d).encode()
        a=subprocess.run([str(old.resolve())],input=payload,capture_output=True,check=True).stdout
        b=subprocess.run([str(new.resolve())],input=payload,capture_output=True,check=True).stdout
        print(f'{d.name}: cycles={len(a.splitlines())}, equal={a==b}')
        if a!=b:
            raise SystemExit('Observed change in approximate replay; inspect before proceeding')


if __name__=='__main__': main()
