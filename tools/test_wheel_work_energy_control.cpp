#include <cassert>
#include <cmath>
#include <initializer_list>
#include "wheel_work_energy_control.h"

int main() {
  using namespace wheel_work_energy_control;
  const State positive_state{54.f,300.f,7350.f,true};
  assert(supported(positive_state,1));
  assert(!positive(positive_state,kPositiveMinQ-.01f).valid);
  const auto p_low=positive(positive_state,9.f);
  const auto p_high=positive(positive_state,15.f);
  assert(p_low.valid && p_high.valid && p_high.energy_mj>p_low.energy_mj);
  Choice positive_choice;
  consider(positive_choice,300,70,9.f,p_low,p_high.energy_mj);
  consider(positive_choice,400,90,15.f,p_high,p_high.energy_mj);
  finish(positive_choice,p_high.energy_mj);
  assert(positive_choice.valid && positive_choice.current_ma==400 &&
      positive_choice.width_ms==90 && !positive_choice.target_above_reachable);

  const State negative_state{68.f,350.f,7400.f,true};
  assert(supported(negative_state,-1));
  Choice negative_choice;
  for (uint16_t width : {30,35,40})
    consider(negative_choice,300,width,NAN,negative(negative_state,width),.733f);
  finish(negative_choice,.733f);
  assert(negative_choice.valid && negative_choice.width_ms==40 &&
      negative_choice.target_below_reachable);
  assert(negative(negative_state,40).energy_mj<negative(negative_state,30).energy_mj);
  assert(!negative(negative_state,45).valid);
  assert(!supported(State{54.f,300.f,7500.f,true},1));
  assert(!supported(State{54.f,300.f,7350.f,false},1));
}
