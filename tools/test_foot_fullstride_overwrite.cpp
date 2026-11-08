#include "foot_fullstride_observer.h"
#include <cassert>
#include <cmath>

using foot_fullstride_observer::Observer;

static void long_cycle(bool lose_left, bool lose_right) {
  Observer o; o.reset(1);
  for (unsigned t=50; t<=3550; t+=50) {
    if (t==50 || t==1750 || t==3400) assert(o.peak(1,-1,t));
    if (t==900 || t==2550) assert(o.peak(1,1,t));
    const bool early = t>=1750 && t<=1950;
    const float angle=(t>=1750 && t<=1850) ? 10.f : 0.f;
    // In the one-foot cases, the other foot has no valid readings among the
    // evicted frames. Its next valid sample is still within the accepted gap.
    const bool lv=!early || lose_left;
    const bool rv=!early || lose_right;
    assert(o.frame(1,t,lv,rv,angle,angle));
  }
  const auto c=o.latest();
  assert(c.sequence==2 && !c.startup);
  assert(c.start_ms==1750 && c.end_ms==3400);
  assert(c.left.valid != lose_left);
  assert(c.right.valid != lose_right);
  assert(c.valid == (!lose_left && !lose_right));
  if(lose_left || lose_right) assert(!c.continuous);
}

static void old_evictions_and_reset() {
  Observer o; o.reset(9);
  for (unsigned t=0; t<=3400; t+=50) {
    if(t%800==0) assert(o.peak(9,-1,t));
    else if(t%800==400) assert(o.peak(9,1,t));
    const float value=(t%800<400) ? 1.f : -1.f;
    assert(o.frame(9,t,true,true,value,value));
  }
  // The ring did wrap, but only old periods were evicted.
  auto c=o.latest(); assert(c.valid && c.left.valid && c.right.valid);
  assert(c.start_ms==2400 && c.end_ms==3200 && std::fabs(c.left.span-2.f)<1e-6f);
  o.reset(10);
  for (unsigned t=0; t<=1800; t+=50) {
    if(t%800==0) assert(o.peak(10,-1,t));
    else if(t%800==400) assert(o.peak(10,1,t));
    assert(o.frame(10,t,true,true,1.f,1.f));
  }
  assert(o.latest().valid);
}

int main() {
  long_cycle(true,true);
  long_cycle(true,false);
  long_cycle(false,true);
  long_cycle(false,false);
  old_evictions_and_reset();
}
