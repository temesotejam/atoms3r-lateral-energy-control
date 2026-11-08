from pathlib import Path
s=Path('src/roller485_manager.cpp').read_text(encoding='utf8')
def body(start,end): return s[s.index(start):s.index(end,s.index(start))]
set_body=body('bool Roller485Manager::setCurrentMa', 'bool Roller485Manager::stop')
apply_body=body('bool Roller485Manager::applyCurrentMa', 'bool Roller485Manager::ok')
assert set_body.index('if (current_mA == 0) return stop();') < set_body.index('cameraExperimentAllowsCurrent(current_mA)') < set_body.index('xQueueSend')
assert apply_body.index('cameraExperimentAllowsCurrent(cmd.current_mA)') < apply_body.index('writeI32(REG_CURRENT, raw)')
# These are the only current/output register write callsites in this manager.
assert s.count('writeI32(REG_CURRENT,') == 2
assert s.count('writeU8(REG_OUTPUT,') == 2
assert 'writeI32(REG_CURRENT, 0)' in s and 'writeU8(REG_OUTPUT, 0)' in s
main=Path('src/main.cpp').read_text(encoding='utf8')
assert '#if CAMERA_DIAGNOSTIC_ONLY\n        // Comparison captures are passive.' in main
assert 'ok = runner.startPassiveCapture();\n#else' in main
runner=Path('src/experiment_runner.cpp').read_text(encoding='utf8')
start=runner[runner.index('bool ExperimentRunner::startPassiveCapture()'):runner.index('bool ExperimentRunner::startEnergyControlV0Capture()')]
assert 'passive_capture_mode_ = true;' in start and 'energy_control_autonomous_mode_ = false;' in start
assert 'status_.pulse_active = false;' in start and 'status_.motor_cmd_mA = 0;' in start
assert 'beginStartSync(millis());' in start
print('passive routing and command guard source contracts PASS; not an on-device test')
