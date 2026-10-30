#include "foot_balance_adaptive.h"
#include <cassert>
#include <cmath>
#include <iostream>
int main() {
  foot_balance_adaptive::Controller c;c.reset(7);
  assert(!c.observe(1,2,true,8,1000,1000));
  assert(!c.observe(1,2,true,7,1000,800));
  assert(!c.observe(1,NAN,true,7,1000,1000));
  for(unsigned i=0;i<3;++i) {
    assert(c.observe(1,2,true,7,1000+i*800,1000+i*800));
    assert(c.observe(-1,.1f,true,7,1400+i*800,1400+i*800));
  }
  assert(std::fabs(c.negativeOffsetDeg()-.1f)<1e-5f);
  assert(!c.observe(-1,.1f,true,7,3000,3000));
  const float before=c.negativeOffsetDeg();
  assert(!c.observe(-1,0,false,7,4000,4000));assert(c.negativeOffsetDeg()==before);
  c.reset(8);assert(c.negativeOffsetDeg()==0);
  for(unsigned i=0;i<30;++i) {
    assert(c.observe(1,.1f,true,8,1000+i*800,1000+i*800));
    assert(c.observe(-1,2.f,true,8,1400+i*800,1400+i*800));
  }
  assert(c.positiveOffsetDeg()>.9f && c.positiveOffsetDeg()<=1.f);
  assert(c.negativeOffsetDeg()==0);
  c.reset(9);
  for(unsigned i=0;i<3;++i)assert(c.observe(1,2,true,9,1000+i*800,1000+i*800));
  for(unsigned i=0;i<3;++i)assert(c.observe(-1,0,true,9,10000+i*800,10000+i*800));
  assert(c.negativeOffsetDeg()==0); // no correction using stale other-side frames
  std::cout<<"Foot-balance adaptive: run, time, valid-frame and bounded adaptation checks PASS\n";
}
