#include <cassert>
#include <cmath>
#include <initializer_list>
#include "wheel_work_energy_control.h"
#include "lateral_energy_control.h"

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

  // Actual 1.0.24 low-voltage / low-wheel-speed failure state. The broad
  // negative model must choose a measured brake input instead of coasting.
  const State low{74.0655f,39.97f,7190.f,true,7.96907f};
  assert(!extendedNegativeSupported(low) && broadNegativeSupported(low));
  Choice broad;
  const auto free=negative(low,0);
  assert(free.valid && free.branch==3 && std::fabs(free.change_mj)<1e-6f);
  for (uint16_t width : {0,10,15,20,25,30,35,40})
    consider(broad,300,width,NAN,negative(low,width),.73342f);
  finish(broad,.73342f);
  assert(broad.valid && broad.branch==3 && broad.width_ms==25 &&
      broad.evaluations==5 && broad.target_below_reachable);
  assert(broad.energy_mj<free.energy_mj && broad.free_mj==free.free_mj);
  assert(std::fabs(broad.energy_mj-(broad.free_mj+.42577061840f*broad.change_mj))<1e-6f);
  assert(!negative(low,30).valid && !negative(low,40).valid);
  assert(std::fabs(broad.work_mj-(broad.on_mj+broad.after_mj))<1e-6f);
  const auto coast=negative(low,0);
  assert(std::fabs(broad.change_mj-(broad.work_mj-coast.work_mj))<1e-6f);
  Choice no_brake;
  for (uint16_t width : {0,10,15,20,25})
    consider(no_brake,300,width,NAN,negative(low,width),free.energy_mj+.01f);
  finish(no_brake,free.energy_mj+.01f);
  assert(no_brake.width_ms==0 && no_brake.target_above_reachable);
  assert(!supported(State{74.f,40.f,7100.f,false,8.f},-1));
  assert(!supported(State{74.f,40.f,7100.f,true},-1));
  assert(!supported(State{74.f,40.f,6200.f,true,8.f},-1));
  assert(supported(State{74.f,40.f,6210.f,true,8.f},-1));
  assert(supported(State{74.f,40.f,8000.f,true,8.f},-1));
  assert(!supported(State{74.f,40.f,8010.f,true,8.f},-1));
  assert(!supported(State{77.f,40.f,7100.f,true,8.f},-1));
  assert(!supported(State{74.f,3.f,7100.f,true,8.f},-1));
  // The original extended branch retains precedence in its measured domain.
  const State overlap{68.f,350.f,7400.f,true,8.f};
  assert(negative(overlap,40).branch==2 && negative(overlap,40).valid);
  assert(!negative(overlap,25).valid);

  // Ground truth from the five held-out late no-input peaks in 42884626.
  // Runtime float math must reproduce the independently fitted baseline.
  const float held[][5] = {
      {74.0655f,39.97f,7190.f,7.96907f,11.82887f},
      {74.2173f,59.86f,7080.f,7.94715f,11.92696f},
      {73.4389f,58.25f,7140.f,8.06085f,11.92007f},
      {73.5625f,41.57f,7150.f,8.08507f,11.81728f},
      {73.2581f,64.96f,7140.f,8.06084f,11.98799f}};
  float squared_error=0.f;
  const lateral_energy::Geometry geometry;
  for (const auto& row : held) {
    const State state{row[0],row[1],row[2],true,row[3]};
    const auto baseline=negative(state,0);
    assert(baseline.valid && baseline.branch==3);
    const float predicted=lateral_energy::angle(geometry,baseline.energy_mj*.001f);
    squared_error+=(predicted-row[4])*(predicted-row[4]);
    Choice selected;
    for (uint16_t width : {0,10,15,20,25,30,35,40})
      consider(selected,300,width,NAN,negative(state,width),.73342f);
    finish(selected,.73342f);
    assert(selected.valid && selected.width_ms==25 && selected.branch==3);
    assert(selected.target_below_reachable);
  }
  assert(std::sqrt(squared_error/5.f)<.17f);
}
