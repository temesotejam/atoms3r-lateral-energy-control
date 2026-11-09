"""Compile and test the actual runner rescue snapshot/restore transaction."""
import subprocess, tempfile, sys
from pathlib import Path
src=(Path(__file__).resolve().parents[1]/"src/experiment_runner.cpp").read_text(encoding="utf8")
start=src.index("bool ExperimentRunner::tryTiltPriorityRescue(")
end=src.index("\nbool ExperimentRunner::tryTiltPriorityImpl(",start)
method=src[start:end]
prefix=r"""
#include <cassert>
#include <cstdint>
#define RESEARCH_FOLLOWUP_RESCUE 1
namespace PsramLogger {struct EnergyControlAutonomousZeroCrossEvent {bool output_executed=false;};}
struct State {int value=0;};
struct ExperimentRunner {
 State tilt_priority_,wheel_cycle_guard_,negative_cap_recovery_;
 uint16_t foot_trial_pending_ms_=17;bool foot_trial_pending_effective_=true;
 bool followup_event_=true,followup_write_uncertain_=false,health=true,sent=false,selected=false;
 bool tryTiltPriorityImpl(PsramLogger::EnergyControlAutonomousZeroCrossEvent& e,uint32_t,bool* healthy){
   *healthy=health;tilt_priority_.value=2;wheel_cycle_guard_.value=2;negative_cap_recovery_.value=2;
   foot_trial_pending_ms_=0;foot_trial_pending_effective_=false;e.output_executed=sent;return selected;}
 bool tryTiltPriorityRescue(PsramLogger::EnergyControlAutonomousZeroCrossEvent&,uint32_t);
};
"""
main=r"""
int main(){
 PsramLogger::EnergyControlAutonomousZeroCrossEvent e;
 ExperimentRunner x; x.negative_cap_recovery_.value=7;
 assert(!x.tryTiltPriorityRescue(e,10));assert(x.negative_cap_recovery_.value==7 && x.foot_trial_pending_ms_==17 && x.foot_trial_pending_effective_);
 ExperimentRunner coast;coast.selected=true;coast.negative_cap_recovery_.value=7;
 assert(coast.tryTiltPriorityRescue(e,10));assert(coast.negative_cap_recovery_.value==7 && coast.foot_trial_pending_ms_==17);
 ExperimentRunner old;old.followup_event_=false;old.negative_cap_recovery_.value=7;
 assert(!old.tryTiltPriorityRescue(e,10));assert(old.negative_cap_recovery_.value==2 && old.foot_trial_pending_ms_==0);
 ExperimentRunner unhealthy;unhealthy.health=false;unhealthy.negative_cap_recovery_.value=7;
 assert(!unhealthy.tryTiltPriorityRescue(e,10));assert(unhealthy.negative_cap_recovery_.value==2);
 ExperimentRunner unknown;unknown.followup_write_uncertain_=true;unknown.negative_cap_recovery_.value=7;
 assert(!unknown.tryTiltPriorityRescue(e,10));assert(unknown.negative_cap_recovery_.value==2);
 ExperimentRunner sent;sent.sent=true;sent.negative_cap_recovery_.value=7;
 assert(!sent.tryTiltPriorityRescue(e,10));assert(sent.negative_cap_recovery_.value==2);
}
"""
with tempfile.TemporaryDirectory() as d:
 p=Path(d)/"x.cpp";p.write_text(prefix+method+main,encoding="utf8")
 exe=Path(d)/("test.exe" if sys.platform=="win32" else "test")
 subprocess.run(["g++","-std=c++11","-Wall","-Wextra","-Werror",str(p),"-o",str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
print("actual runner followup no-send transaction PASS")
