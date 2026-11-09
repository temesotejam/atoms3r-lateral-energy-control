"""Hypothesis-only 3D foot-gap calculation; no contact forces or motor output.

Coordinates: x lateral, y fore/aft, z up; body sway about y, pitch about x.
Separate foot joints rotate about x at height H; sole is the 150mm split arc.
Independent leg angles and hip height are inputs, not identified by this code.
"""
import math

R=150.0
INNER=5.0
OUTER=45.0
WIDTH=56.0

def min_height(side, sway_deg, pitch_deg, leg_deg, hip_height_mm=120.0):
    sway,pitch,leg=map(math.radians,(sway_deg,pitch_deg,leg_deg))
    a=math.sqrt(R*R-INNER*INNER)
    c=math.cos(pitch+leg)
    assert c>0 and math.cos(sway)>0
    k=math.tan(sway)/c
    x_unclamped=R*k/math.sqrt(1+k*k)
    lo,hi=(-OUTER,-INNER) if side<0 else (INNER,OUTER)
    x=max(lo,min(hi,x_unclamped))
    z=a-math.sqrt(R*R-x*x)
    return (-math.sin(sway)*x+math.cos(sway)*(hip_height_mm*math.cos(pitch)
            -abs(math.sin(pitch+leg))*WIDTH/2+c*(z-hip_height_mm)))

def gap(sway,pitch,left,right,H=120.):
    return min_height(-1,sway,pitch,left,H)-min_height(1,sway,pitch,right,H)

def switch(pitch,left,right,H=120.):
    # Only report a geometric equal-height root, not force transfer or touchdown.
    lo,hi=-15.,15.
    if gap(lo,pitch,left,right,H)*gap(hi,pitch,left,right,H)>0:return None
    for _ in range(45):
        mid=(lo+hi)/2
        if gap(lo,pitch,left,right,H)*gap(mid,pitch,left,right,H)<=0:hi=mid
        else:lo=mid
    return (lo+hi)/2

if __name__=='__main__':
    for pitch in (0,2,-2):
        for left,right in ((0,0),(0,2),(2,0),(-2,2),(0,5)):
            print('pitch, left leg, right leg, predicted equal-height roll [deg]:',pitch,left,right,round(switch(pitch,left,right),3))
