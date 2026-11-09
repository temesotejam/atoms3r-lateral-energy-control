#include "../src/rescue_rebound.h"
#include <cassert>
#include <cmath>
using rescue_rebound::Detector;
static bool feed(Detector& d,uint32_t start,float v,float end,int side=1) {
  bool hit=false;
  for(uint32_t t=start;t<=end;t+=2) {
    const float a=v;
    const auto p=d.observe(t,a,a,side*3.f,true);
    hit|=p.valid;
  }
  return hit;
}
static bool trial(Detector& d,uint32_t t,bool accept=true) {
  bool hit=false;
  for(int i=1;i<=100;i++) {float a=2.f-i*.01f; auto p=d.observe(t+i*2,a,a,-5.f,true);hit|=p.valid;}
  for(int i=1;i<=250;i++) {float a=1.f+i*.016f;auto p=d.observe(t+200+i*2,a,a,8.f,true);hit|=p.valid;}
  for(int i=1;i<=25;i++) {float a=5.f-i*.04f;const uint32_t now=t+700+i*2;auto p=d.observe(now,a,a,-8.f,true);hit|=p.valid;if(p.valid)d.resolve(accept,now,a);}
  return hit;
}
int main(){
 Detector d; d.arm(1000,1);assert(trial(d,1000));assert(d.suppressRescue());
 d.arm(2000,1);assert(!trial(d,2000)); // no second rescue until a real crossing
 d.normalCrossing();d.arm(4000,1);assert(trial(d,4000));
 d.reset();d.arm(1000,1);for(int i=1;i<=100;i++){float a=2.f-i*.025f;assert(!d.observe(1000+i*2,a,a,-3.f,true).valid);}assert(!trial(d,1200));
 d.reset();d.arm(1000,1);assert(!feed(d,1002,1.f,1180));assert(!d.suppressRescue());
 d.reset();d.arm(1000,1);assert(!d.observe(1002,1.f,1.f,0,false).valid);assert(!trial(d,1002));
 d.reset();d.arm(1000,1);assert(!d.observe(1030,1.f,1.f,0,true).valid);assert(!trial(d,1030));
 d.reset();d.arm(1000,-1);assert(!trial(d,1000));
 d.reset();d.arm(1000,1);assert(!d.observe(1002,1.f,2.f,0,true).valid);assert(!trial(d,1002));
 d.reset();d.arm(1000,1);assert(trial(d,1000,false));assert(!d.suppressRescue());
 // A rejected candidate must not be repeatedly yielded from the old peak.
 for(uint32_t t=1752;t<1800;t+=2)assert(!d.observe(t,4.f,4.f,-3.f,true).valid);
}
