"""Check that the real runner keeps recovery distinct from ordinary events."""
from pathlib import Path
root=Path(__file__).resolve().parents[1]
s=(root/'src/experiment_runner.cpp').read_text(encoding='utf8')
motion=s.split('void ExperimentRunner::updateEnergyControlAutonomousMotion(',1)[1].split('\nvoid ExperimentRunner::updateEnergyControlAutonomousPeakTracker(',1)[0]
for token in ('stalled_return_.update(', 'energy_control_autonomous_last_peak_ms_',
              'threshold_deg,enabled', '!crossing && !confirmation_input',
              'stalled_return_.interrupt()',
              'stalled_return::reacquireDue(',
              'energy_control_autonomous_pending_peak_=false', 'resetEnergyControlAutonomousPeakTracker(true)'):
    assert token in motion, token
method=s.split('bool ExperimentRunner::tryTiltPriority(',1)[1].split('\nbool ExperimentRunner::tryWeakSideAssist(',1)[0]
for token in ('choice.width_ms>30)choice.width_ms=30',
              'event.work_control_branch=return_recovery ? 16 : 15',
              '!return_recovery && !choice.coast', 'stalled_return_awaiting_peak_=return_recovery'):
    assert token in method, token
cycle=s.split('void ExperimentRunner::processFootSpanCycle()',1)[1].split('\nvoid ExperimentRunner::recordFootPhasePeak(',1)[0]
assert 'n.branch!=16' in cycle
assert 'stalled_return_.reset()' in s
print('stalled return crossing, bounded dispatch, timeout rearm and learning isolation PASS')
