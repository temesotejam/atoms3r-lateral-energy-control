#include "period2_observer.h"
#include <cassert>
using namespace period2_observer;
static Pair pair(uint32_t t,int side,bool high){
 Pair p;p.strong_decision_ms=t;p.strong_peak_ms=t+150;p.weak_decision_ms=t+350;p.weak_peak_ms=t+550;
 p.strong_side=side;p.stage_target_deg=4.5f;p.strong_deg=high?6.5f:4.4f;p.weak_deg=high?2.8f:4.2f;
 p.strong_outcome=Outcome::Sent;p.weak_outcome=high?Outcome::IntentionalCoast:Outcome::Sent;return p;
}
static void mirror(int side){
 Observer x;Detection d;
 for(int i=0;i<4;i++){auto p=pair(1000+i*800,side,i%2==0);d=x.observe(p,p.weak_peak_ms);if(i<3)assert(!d.valid);}
 assert(d.valid && d.within_stage_plus_2p5 && d.strong_side==side && d.end_ms==3950);
}
int main(){
 mirror(-1);mirror(1);
 for(int variant=0;variant<5;++variant){
  Observer x;Detection d;
  for(int i=0;i<4;i++){
   auto p=pair(1000+i*800,-1,i%2==0);
   if(i==2 && variant==0)p.weak_outcome=Outcome::Sent; // overshoot without coast
   if(i==2 && variant==1)p.weak_deg=4.4f; // opposite side not weak
   if(i==2 && variant==2)p.strong_outcome=Outcome::Missing; // unknown or failed send
   if(i==2 && variant==3)p.strong_deg=5.9f; // no overshoot
   if(i==2 && variant==4)p.stage_target_deg=5.5f; // changed stage
   d=x.observe(p,p.weak_peak_ms);
  }
  assert(!d.valid);
 }
 Observer x;auto p=pair(1000,1,true);assert(!x.observe(p,p.weak_peak_ms).valid);
 assert(!x.observe(p,p.weak_peak_ms).valid); // duplicate invalidates history
 for(int i=1;i<4;++i){p=pair(1000+i*800,1,i%2==0);assert(!x.observe(p,p.weak_peak_ms).valid);}
 Observer stale;
 for(int i=0;i<4;++i){p=pair(1000+i*2300,1,i%2==0);assert(!stale.observe(p,p.weak_peak_ms).valid);}
 Observer unordered;p=pair(1000,1,true);p.weak_decision_ms=p.strong_peak_ms-1;assert(!unordered.observe(p,p.weak_peak_ms).valid);
 Observer notyet;p=pair(1000,1,true);assert(!notyet.observe(p,p.weak_peak_ms-1).valid);
 Observer outside; Detection d;
 for(int i=0;i<4;i++){p=pair(1000+i*800,-1,i%2==0);if(i==0)p.strong_deg=7.1f;d=outside.observe(p,p.weak_peak_ms);}
 assert(d.valid && !d.within_stage_plus_2p5);
}
