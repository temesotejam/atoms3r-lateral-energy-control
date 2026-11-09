#include "period2_observer.h"
#include <iostream>
using namespace period2_observer;
int main(){
 Observer plus,minus; int side,strong_status,weak_status;
 uint32_t now,sd,sp,wd,wp;float target,strong,weak;
 while(std::cin>>side>>now>>sd>>sp>>wd>>wp>>target>>strong>>weak>>strong_status>>weak_status){
  if(side==0){plus.reset();minus.reset();continue;}
  Pair p;p.strong_side=static_cast<int8_t>(side);p.strong_decision_ms=sd;p.strong_peak_ms=sp;
  p.weak_decision_ms=wd;p.weak_peak_ms=wp;p.stage_target_deg=target;p.strong_deg=strong;p.weak_deg=weak;
  p.strong_outcome=static_cast<Outcome>(strong_status);p.weak_outcome=static_cast<Outcome>(weak_status);
  const Detection r=(side>0?plus:minus).observe(p,now);
  if(r.valid)std::cout<<static_cast<int>(r.strong_side)<<" "<<r.end_ms<<" "<<r.within_stage_plus_2p5<<"\n";
 }
}
