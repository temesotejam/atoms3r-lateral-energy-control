"""Guard position observation and the pre-probe controller math."""
from pathlib import Path
import subprocess

root=Path(__file__).resolve().parents[1]
baseline='79de6323dbd08ec1ec8920d6df11446bbaf2d7d6'
allowed={
    'src/config.h':[
        'static constexpr uint32_t WHEEL_POSITION_READ_PERIOD_US = 5000UL;\n',
        'static constexpr uint16_t WHEEL_POSITION_READ_TIMEOUT_MS = 2;\n'],
}
unchanged=['src/lateral_energy_control.h','src/peak_error_adaptation.h',
           'src/previous_peak_control_correction.h','src/control_math_cache.h',
           'src/mekf6.cpp','src/mekf6.hpp',
           'src/upright_reference_store.cpp','src/upright_reference.h']
for name in list(allowed)+unchanged:
    before=subprocess.check_output(['git','show',baseline+':'+name],cwd=root).decode().replace('\r\n','\n')
    after=(root/name).read_text(encoding='utf-8')
    for line in allowed.get(name,[]):
        assert after.count(line)==1,(name,line)
        after=after.replace(line,'')
    assert before==after,'unauthorized controller change: '+name
runner_before=subprocess.check_output(['git','show',baseline+':src/experiment_runner.cpp'],cwd=root).decode().replace('\r\n','\n')
runner_after=(root/'src/experiment_runner.cpp').read_text(encoding='utf-8')
def legacy_decision(source):
    return source.split('  auto decide_selected=[&]',1)[1].split('    return energy_bridge_enabled_',1)[1].split('  };',1)[0]
# The experiment chooses a table pointer only for randomized positive-current
# trials. All other modes keep the original 300 mA table and decision math.
assert 'const lateral_energy::CurrentTable* selected_current_table = &lateral_current_table_;' in runner_after
assert 'if (positiveCurrentBatchMode())\n    selectPositiveCurrentBatch(event, selected_current_mA, selected_current_table);' in runner_after
assert 'if (!positiveCurrentBatchMode() || event.physical_next_peak_side <= 0) return;' in runner_after
assert legacy_decision(runner_before)==legacy_decision(runner_after).replace('*selected_current_table','lateral_current_table_'), \
    'legacy bridge/quadratic decision changed during inverse-mode addition'
assert '      observation.position_sequence != last_logged_position_sequence_ ||\n' in runner_after
assert '  last_logged_position_sequence_ = roller_telemetry.position_sequence;\n' in runner_after
assert '  uint32_t last_logged_position_sequence_ = 0;\n' in \
    (root/'src/experiment_runner.h').read_text(encoding='utf-8')
source=(root/'src/roller485_manager.cpp').read_text()
position=source.split('bool Roller485Manager::readPositionFresh() {',1)[1].split('bool Roller485Manager::readSpeedFresh()',1)[0]
assert 'readI32(REG_POSITION_READBACK, raw)' in position
assert 'recordIo(' not in position and 'setCurrentMa(' not in position and 'stop()' not in position
update=source.split('void Roller485Manager::update() {',1)[1].split('bool Roller485Manager::setCurrentMa',1)[0]
assert update.index('requested_current_mA_ != command_mA_')<update.index('readPositionFresh()')
assert 'if (requested_current_mA_ != command_mA_) return;\n  if (last_speed_due_us_' in update
print('Legacy decision math, MEKF and position observation guarded; inverse-mode addition allowed PASS')
