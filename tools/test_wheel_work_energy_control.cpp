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

  const State state{69.2351f,218.97f,7150.f,true,7.88428f};
  assert(supported(state,-1));
  const auto no_input=negative(state,0);
  assert(no_input.valid && no_input.branch==4 && std::fabs(no_input.change_mj)<1e-6f);
  float last=no_input.energy_mj;
  Choice all;
  for (uint16_t width=0;width<=40;width=(width==0 ? 10 : width+1)) {
    const auto p=negative(state,width);
    assert(p.valid && p.energy_mj<=last);
    assert(std::fabs(p.work_mj-p.on_mj-p.after_mj)<1e-6f);
    assert(std::fabs(p.change_mj-(p.work_mj-no_input.work_mj))<1e-6f);
    assert(std::fabs(p.energy_mj-(p.free_mj+.44113787836f*p.change_mj))<1e-6f);
    assert(p.cross_condition==(width>25));
    consider(all,300,width,NAN,p,.73342f);
    last=p.energy_mj;
  }
  finish(all,.73342f);
  assert(all.valid && all.width_ms==40 && all.target_below_reachable);
  assert(!negative(state,41).valid && !negative(state,1).valid);
  assert(!negative(state,25,NAN).valid);
  assert(negative(state,25).on_mj<0 && negative(state,25).after_mj>0);
  assert(std::fabs(negative(state,25,.05f).energy_mj-negative(state,25).energy_mj-.05f)<1e-6f);
  const State measured_strong{68.f,350.f,7400.f,true,8.f};
  assert(!negative(measured_strong,40).cross_condition);
  assert(!supported(State{74.f,40.f,7100.f,false,8.f},-1));
  assert(!supported(State{74.f,40.f,7100.f,true},-1));
  assert(!supported(State{74.f,40.f,6200.f,true,8.f},-1));
  assert(supported(State{74.f,40.f,6210.f,true,8.f},-1));
  assert(supported(State{74.f,40.f,8000.f,true,8.f},-1));
  assert(!supported(State{74.f,40.f,8010.f,true,8.f},-1));
  assert(!supported(State{77.f,40.f,7100.f,true,8.f},-1));
  assert(!supported(State{74.f,3.f,7100.f,true,8.f},-1));

  PositiveFeedback positive_feedback;
  assert(positive_feedback.biasMj()==0.f);
  for (unsigned i=0;i<12;++i) {
    const float raw=.8f;
    const float corrected=raw+positive_feedback.biasMj();
    // Runtime strips previously applied bias before observing again.
    positive_feedback.observe(.74f,corrected-positive_feedback.biasMj());
  }
  assert(std::fabs(positive_feedback.biasMj()+.06f)<1e-6f);
  const auto valid_count=positive_feedback.count();
  positive_feedback.observe(NAN,.8f);
  positive_feedback.observe(.7f,NAN);
  assert(positive_feedback.count()==valid_count);
  PositiveFeedback separate_work_branch;
  assert(separate_work_branch.biasMj()==0.f);
  for (unsigned i=0;i<3;++i) positive_feedback.observe(.2f,.8f);
  assert(std::fabs(positive_feedback.biasMj()+.12f)<1e-6f);
  positive_feedback=PositiveFeedback{};
  assert(positive_feedback.count()==0 && positive_feedback.biasMj()==0.f);
  // Underprediction and overprediction change the energy inverse in opposite directions.
  lateral_energy::CurrentTable table;
  assert(table.prepare(.07f));
  const lateral_energy::Geometry g;
  const float target=lateral_energy::potential(g,8.f);
  const auto nominal=lateral_energy::decideBridge(g,table,1,54.f,target,0.f,0.f,300.f);
  const auto more=lateral_energy::decideBridge(g,table,1,54.f,target,0.f,0.f,300.f,-.00006f);
  const auto less=lateral_energy::decideBridge(g,table,1,54.f,target,0.f,0.f,300.f,.00006f);
  assert(nominal.valid && more.valid && less.valid);
  assert(more.requested_q_mAs>nominal.requested_q_mAs);
  assert(less.requested_q_mAs<nominal.requested_q_mAs);

  NegativeFeedback feedback;
  assert(feedback.capMs(8.f)==25 && feedback.biasMj()==0.f);
  feedback.observe(1.14f,1.08f,.733f,8.f,25);
  assert(feedback.capMs(8.f)==25 && feedback.biasMj()==0.f);
  feedback.observe(1.14f,1.08f,.733f,8.f,25);
  assert(feedback.capMs(8.f)==30 && std::fabs(feedback.biasMj()-.06f)<1e-6f);
  feedback.observe(1.14f,1.08f,.733f,8.f,25);
  assert(feedback.capMs(8.f)==30); // No growth unless the current cap was used.
  for (uint16_t cap : {30,35}) {
    feedback.observe(1.1f,1.05f,.733f,8.f,cap);
    assert(feedback.capMs(8.f)==cap);
    feedback.observe(1.1f,1.05f,.733f,8.f,cap);
    assert(feedback.capMs(8.f)==cap+5);
  }
  for (unsigned i=0;i<20;++i) feedback.observe(1.1f,1.05f,.733f,8.f,40);
  assert(feedback.capMs(8.f)==40 && feedback.capMs(7.4f)==25);
  feedback.observe(.65f,.65f,.733f,8.f,40);
  assert(feedback.capMs(8.f)==35);
  feedback.observe(1.1f,1.05f,.733f,7.4f,25);
  assert(feedback.capMs(8.f)==25);
  const auto samples=feedback.count();
  feedback.observe(NAN,1.0f,.733f,8.f,25);
  assert(feedback.count()==samples && feedback.capMs(8.f)==25);
  for (unsigned i=0;i<4;++i) feedback.observe(1.5f,1.f,.733f,8.f,25);
  assert(std::fabs(feedback.biasMj()-.12f)<1e-6f);

  NegativeFeedback consistent;
  for (unsigned i=0;i<12;++i) {
    const float applied_bias=consistent.biasMj();
    const float corrected_prediction=1.f+applied_bias;
    consistent.observe(1.05f,corrected_prediction-applied_bias,.733f,8.f,25);
  }
  assert(std::fabs(consistent.biasMj()-.05f)<1e-6f);

  // Runtime float regression against the latest five measured 25-ms peaks.
  // Whole-run held-out accuracy is checked separately by the fitting script.
  const float actual[][5] = {
      {69.2351f,218.97f,7150.f,7.88428f,10.34158f},
      {69.9325f,224.45f,7150.f,7.92085f,10.43093f},
      {69.0678f,251.69f,7160.f,7.98930f,10.55294f},
      {69.3050f,245.62f,7150.f,7.97437f,10.48583f},
      {70.7654f,236.37f,7150.f,7.95699f,10.51178f}};
  float squared_error=0.f;
  const lateral_energy::Geometry geometry;
  for (const auto& row : actual) {
    const State x{row[0],row[1],row[2],true,row[3]};
    const auto p=negative(x,25);
    const float predicted=lateral_energy::angle(geometry,p.energy_mj*.001f);
    squared_error+=(predicted-row[4])*(predicted-row[4]);
    assert(negative(x,40).energy_mj<p.energy_mj);
  }
  assert(std::sqrt(squared_error/5.f)<.15f);

  // Causal replay of recorded positive peaks, run 107786469 (1.0.26).
  // Evaluates prediction correction on the original inputs, not a closed-loop simulation.
  const float recorded[][3]={
      {864.f,0.288500000f,0.201410000f},
      {1663.f,0.288520000f,0.227970000f},
      {2426.f,0.290290000f,0.257420000f},
      {3155.f,0.350790000f,0.290060000f},
      {3876.f,0.451170000f,0.353810000f},
      {4646.f,0.537680000f,0.411600000f},
      {5484.f,0.631520000f,0.517240000f},
      {6415.f,0.734680000f,0.649670000f},
      {7338.f,0.735190000f,0.654170000f},
      {8261.f,0.741650000f,0.671700000f},
      {9178.f,0.740310000f,0.661750000f},
      {10097.f,0.744960000f,0.670820000f},
      {11014.f,0.747410000f,0.692510000f},
      {11928.f,0.747070000f,0.697520000f},
      {12841.f,0.742950000f,0.695930000f},
      {13760.f,0.751190000f,0.700480000f},
      {14688.f,0.749390000f,0.672960000f},
  };
  PositiveFeedback replay;
  float before=0.f,after=0.f;
  unsigned evaluated=0;
  for (const auto& row : recorded) {
    const float bias=replay.biasMj(); // only earlier completed peaks
    if (row[0]>=10000.f) {
      before+=(row[2]-row[1])*(row[2]-row[1]);
      after+=(row[2]-row[1]-bias)*(row[2]-row[1]-bias);
      ++evaluated;
    }
    replay.observe(row[2],row[1]);
  }
  assert(evaluated>=5 && after<before*.5f);
}
