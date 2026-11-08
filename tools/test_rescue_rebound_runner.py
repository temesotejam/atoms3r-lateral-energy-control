"""Compile runner methods extracted from source with a controllable test double.

Tests acceptance/rejection and zero-cross transactions. Other runner methods,
hardware IO and event logging are mocked and still require full integration.
"""
import subprocess
import tempfile
from pathlib import Path
root = Path(__file__).resolve().parents[1]
source = (root/'src/experiment_runner.cpp').read_text(encoding='utf8')
def extract(sig):
    start=source.index(sig)
    opening=source.index('{',start); depth=0
    for i in range(opening,len(source)):
        if source[i]=='{': depth+=1
        if source[i]=='}':
            depth-=1
            if depth==0:return source[start:i+1]
    raise ValueError(sig)
peak = extract('bool ExperimentRunner::acceptRescueRebound(')
cross = extract('__attribute__((noinline)) void ExperimentRunner::acceptMotionCrossing(')
prefix = r'''
#include <cassert>
#include <cmath>
#include <cstdint>
#define RESEARCH_RESCUE_REBOUND 1
#define RESEARCH_FOLLOWUP_RESCUE 1
namespace rescue_rebound { struct Peak {bool valid=false;uint32_t ms=0;float angle=0,detector_angle=0;}; }
struct Detector {int calls=0;void reset(){++calls;} };
struct MockRebound {int accepted=0,rejected=0,cross=0,armed=0;
 void resolve(bool b,uint32_t,float){b?++accepted:++rejected;}
 void normalCrossing(){++cross;} void arm(uint32_t,int8_t){++armed;}};
struct Followup {int arms=0,cancels=0;void arm(uint32_t,float){++arms;} void cancel(){++cancels;}};
struct Status {bool pulse_active=false,emergency_stop=false;};
class ExperimentRunner {public:
 Status status_; MockRebound rescue_rebound_; Detector stall_rescue_detector_; Followup followup_detector_;
 bool energy_control_autonomous_pending_peak_=false,rescue_rebound_recording_=false;
 bool energy_control_autonomous_last_accepted_zero_cross_valid_=true,stall_rescue_event_=false;
 uint32_t energy_control_autonomous_last_accepted_zero_cross_ms_=100,stall_rescue_attempted_peak_ms_=0;
 uint32_t energy_control_autonomous_last_peak_ms_=20;int8_t energy_control_autonomous_last_peak_side_=1;
 bool record_ok=true,accept_cross=false,start_pulse=false;int records=0;
 bool recordEnergyControlAutonomousPeak(uint32_t,int8_t,float,float){++records;return record_ok;}
 void updateEnergyControlAutonomousAtZeroCross(uint32_t t,float,float,float,float,float){if(accept_cross){energy_control_autonomous_last_accepted_zero_cross_ms_=t;status_.pulse_active=start_pulse;}}
 bool acceptRescueRebound(const rescue_rebound::Peak&,uint32_t,float,bool);
 void acceptMotionCrossing(bool,uint32_t,float,float,float,float,float);
};
'''
main = r'''
int main(){
 ExperimentRunner r;rescue_rebound::Peak p;p.valid=true;p.ms=200;p.angle=5;p.detector_angle=5;
 r.energy_control_autonomous_pending_peak_=true;r.record_ok=false;
 assert(!r.acceptRescueRebound(p,210,4,true));
 assert(r.energy_control_autonomous_pending_peak_ && !r.rescue_rebound_recording_ && r.rescue_rebound_.rejected==1);
 r.record_ok=true;assert(r.acceptRescueRebound(p,212,4,true));
 assert(!r.energy_control_autonomous_pending_peak_ && !r.rescue_rebound_recording_ && r.rescue_rebound_.accepted==1 && r.followup_detector_.arms==1);
 r.status_.pulse_active=true;assert(!r.acceptRescueRebound(p,214,4,true));assert(r.records==2 && r.rescue_rebound_.rejected==2);
 r.status_.pulse_active=false;r.accept_cross=false;
 r.acceptMotionCrossing(false,220,1,1,0,1,220);assert(r.rescue_rebound_.cross==0); // rejected zero
 r.accept_cross=true;r.acceptMotionCrossing(false,240,1,1,0,1,240);assert(r.rescue_rebound_.cross==1 && r.followup_detector_.cancels==1); // accepted zero
 r.energy_control_autonomous_pending_peak_=true;r.start_pulse=true;
 r.acceptMotionCrossing(true,500,1,1,0,1,500);assert(r.rescue_rebound_.armed==1);
}
'''
with tempfile.TemporaryDirectory() as td:
    p=Path(td)/'fixture.cpp';p.write_text(prefix+'\n'+peak+'\n'+cross+'\n'+main,encoding='utf8')
    out=Path(td)/('fixture.exe' if __import__('sys').platform=='win32' else 'fixture')
    subprocess.run(['g++','-std=c++11','-Wall','-Wextra','-Werror',str(p),'-o',str(out)],check=True)
    subprocess.run([str(out)],check=True)
print('runner extracted rebound transaction and rejected/accepted ZERO PASS')
