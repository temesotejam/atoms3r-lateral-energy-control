#include "stalled_return.h"
#include <cassert>
#include <iostream>
using stalled_return::Detector;
int main(){
  // A timed-out recovery with no matching peak rearms tracking; a completed
  // or ordinary pulse cannot use that path, including at uint wrap/order.
  assert(!stalled_return::reacquireDue(true,true,true,2200,1000));
  assert(stalled_return::reacquireDue(true,true,true,2201,1000));
  assert(!stalled_return::reacquireDue(false,true,true,2201,1000));
  assert(!stalled_return::reacquireDue(true,false,true,2201,1000));
  assert(!stalled_return::reacquireDue(true,true,false,2201,1000));
  assert(!stalled_return::reacquireDue(true,true,true,999,1000));
  // 494: positive peak 13.660s, slowed while still at +0.34deg. A confirmed
  // threshold crossing would take precedence over this bounded trial.
  Detector a;
  assert(!a.update(14018,13660,7.45,1,.52f,-8.f,0,true));
  assert(!a.update(14028,13660,7.45,1,.46f,-7.f,0,true));
  assert(!a.update(14038,13660,7.45,1,.41f,-6.f,0,true));
  assert(a.update(14048,13660,7.45,1,.34f,-4.68f,0,true));
  assert(!a.update(14080,13660,7.45,1,.30f,-4.f,0,true));
  // A momentarily ineligible sample resets dwell but cannot re-arm an
  // already-fired peak; a new measured peak is required.
  a.interrupt();assert(!a.update(14090,13660,7.45,1,.28f,-4.f,0,true));
  // 119: the original early threshold was above zero and was not reached.
  Detector b;
  assert(!b.update(27170,26847,9.20f,1,3.55f,-9.f,2.22f,true));
  assert(!b.update(27180,26847,9.20f,1,3.47f,-8.f,2.22f,true));
  assert(!b.update(27190,26847,9.20f,1,3.40f,-7.f,2.22f,true));
  assert(b.update(27200,26847,9.20f,1,3.25f,-4.99f,2.22f,true));
  // Do not pre-empt a crossing expected within 50ms at current speed.
  Detector near;
  assert(!near.update(11000,10600,7.f,1,.35f,-9.f,0,true));
  assert(!near.update(11020,10600,7.f,1,.20f,-8.f,0,true));
  assert(!near.update(11040,10600,7.f,1,.04f,-5.f,0,true));
  // Missing sensor intervals cannot be mistaken for 30ms of sustained motion.
  Detector gap;assert(!gap.update(14000,13660,7.f,1,.8f,-5.f,0,true));
  gap.interrupt();assert(!gap.update(14010,13660,7.f,1,.7f,-5.f,0,true));
  assert(!gap.update(14100,13660,7.f,1,.5f,-5.f,0,true));
  assert(!gap.update(14110,13660,7.f,1,.47f,-5.f,0,true));
  assert(!gap.update(14120,13660,7.f,1,.45f,-5.f,0,true));
  assert(gap.update(14130,13660,7.f,1,.4f,-5.f,0,true));
  // Direction symmetry, real conditions and reset.
  Detector neg;assert(!neg.update(1000,700,7.f,-1,-1.f,5.f,0,true));
  assert(!neg.update(1010,700,7.f,-1,-.95f,5.f,0,true));
  assert(!neg.update(1020,700,7.f,-1,-.9f,5.f,0,true));
  assert(neg.update(1030,700,7.f,-1,-.8f,5.f,0,true));
  neg.reset();assert(!neg.update(1031,700,7.f,-1,-.7f,5.f,0,false));
  assert(!neg.update(1050,700,7.f,-1,-.7f,5.f,0,true));
  assert(!neg.update(1060,700,7.f,-1,-.6f,-5.f,0,true));
  assert(!neg.update(1090,700,7.f,-1,-.5f,5.f,0,true));
  std::cout<<"stalled return: bounded once per peak, crossing priority and missing samples PASS\n";
}
