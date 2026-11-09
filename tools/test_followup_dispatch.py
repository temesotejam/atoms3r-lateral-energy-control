"""Extract actual runner dispatch and verify experiment write-failure handling.

Other runner methods are mocked; this is not a full real-time or hardware test.
"""
import subprocess
import tempfile
from pathlib import Path

root = Path(__file__).resolve().parents[1]
source = (root / 'src/experiment_runner.cpp').read_text(encoding='utf8')
start = source.index('__attribute__((noinline)) bool ExperimentRunner::dispatchTiltPriority(')
opening = source.index('{', start)
depth = 0
for i in range(opening, len(source)):
    if source[i] == '{': depth += 1
    if source[i] == '}':
        depth -= 1
        if depth == 0:
            method = source[start:i + 1]
            break
else:
    raise ValueError('dispatch method not closed')

prefix = r'''
#include <cassert>
#include <cstdint>
#define RESEARCH_FOLLOWUP_RESCUE 1
namespace Config { constexpr int ENERGY_CONTROL_AUTONOMOUS_REASON_VALID_NO_OUTPUT=9; }
namespace PsramLogger { struct EnergyControlAutonomousZeroCrossEvent {
  float target_peak_deg=8.f;bool output_executed=false,valid=true;int reason=0;
}; }
struct Logger {int count=0;void addEnergyControlAutonomousZeroCrossEvent(const PsramLogger::EnergyControlAutonomousZeroCrossEvent&){++count;}};
class ExperimentRunner {public:
 enum class EnergyControlAutonomousHalfCycleState { WAIT_ZERO_CROSS,WAIT_PEAK };
 Logger storage,*logger_=&storage;bool followup_event_=true,followup_write_uncertain_=false,stall_rescue_event_=true;
 bool select=true,sent=false,emergency=false;int restore=0,finish=0;
 EnergyControlAutonomousHalfCycleState energy_control_autonomous_half_cycle_state_=EnergyControlAutonomousHalfCycleState::WAIT_ZERO_CROSS;
 bool wheelWorkMode(){return true;}
 bool tryTiltPriority(PsramLogger::EnergyControlAutonomousZeroCrossEvent& e,uint32_t){e.output_executed=sent;return select;}
 void requestEmergencyStop(const char*){emergency=true;}
 void restoreUnsentStallRescue(PsramLogger::EnergyControlAutonomousZeroCrossEvent&,bool,uint32_t){++restore;}
 void finishFootEnergyDecision(PsramLogger::EnergyControlAutonomousZeroCrossEvent&,uint32_t){++finish;}
 void resetEnergyControlAutonomousPeakTracker(bool){}
 bool dispatchTiltPriority(PsramLogger::EnergyControlAutonomousZeroCrossEvent&,uint32_t,bool,uint32_t);
};
'''
main = r'''
int main(){
 PsramLogger::EnergyControlAutonomousZeroCrossEvent e;
 ExperimentRunner uncertain;uncertain.followup_write_uncertain_=true;
 assert(uncertain.dispatchTiltPriority(e,100,true,50));
 assert(uncertain.emergency && uncertain.storage.count==1 && uncertain.restore==0 && uncertain.finish==0);
 ExperimentRunner definite;definite.select=false;
 assert(definite.dispatchTiltPriority(e,100,true,50));assert(!definite.emergency && definite.restore==1);
 ExperimentRunner coast;coast.sent=false;
 assert(coast.dispatchTiltPriority(e,100,true,50));assert(!coast.emergency && coast.restore==1);
 ExperimentRunner sent;sent.sent=true;
 assert(sent.dispatchTiltPriority(e,100,true,50));assert(!sent.emergency && sent.finish==1 && sent.restore==0);
}
'''
with tempfile.TemporaryDirectory() as td:
    p = Path(td) / 'test.cpp'
    p.write_text(prefix + method + main, encoding='utf8')
    exe = Path(td) / ('test.exe' if __import__('sys').platform == 'win32' else 'test')
    subprocess.run(['g++', '-std=c++11', '-Wall', '-Wextra', '-Werror', str(p), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
print('actual runner dispatch: uncertain write stops, no-send restores, sent proceeds PASS')
