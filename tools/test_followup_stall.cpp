#include "../src/followup_stall.h"
#include <cassert>
#include <limits>
using followup_stall::Detector;
int main(){
  Detector d; assert(!d.arm(100,4.99f)); assert(d.arm(14370,5.1f));
  assert(!d.observe(14760,14754,.36f,.33f,-6.49f,-5.82f,true,false));
  assert(!d.observe(14762,14760,.33f,.30f,-5.82f,-5.69f,true,false));
  assert(d.observe(14767,14762,.30f,.27f,-5.69f,-4.96f,true,false));
  assert(!d.arm(15000,6.f));
  d.reset();assert(d.arm(14370,5.1f));
  assert(!d.observe(14760,14754,.36f,.33f,-6.49f,-5.82f,false,false));
  assert(!d.observe(14767,14760,.33f,.27f,-5.82f,-4.96f,true,false));
  d.reset();assert(d.arm(14370,5.1f));
  assert(!d.observe(14760,14754,.36f,.33f,-6.49f,-5.82f,true,true));
  d.reset();assert(d.arm(14370,5.1f));
  assert(!d.observe(14760,14754,.36f,-.01f,-6.49f,-5.82f,true,false));
  d.reset();assert(d.arm(14370,5.1f));
  assert(!d.observe(14760,14740,.36f,.33f,-6.49f,-5.82f,true,false));
  d.reset();assert(d.arm(14370,5.1f));
  assert(!d.observe(14760,14754,.36f,std::numeric_limits<float>::quiet_NaN(),-6.49f,-5.82f,true,false));
  using followup_stall::commandAllowed;
  assert(commandAllowed(.25f,.20f,-5.f,5000,5,true));
  assert(!commandAllowed(.25f,0.f,-5.f,0,0,true));
  assert(!commandAllowed(.25f,-.01f,-5.f,0,0,true));
  assert(!commandAllowed(.25f,.76f,-5.f,0,0,true));
  assert(!commandAllowed(.25f,.2f,-5.f,5001,0,true));
  assert(!commandAllowed(.25f,.2f,-5.f,0,6,true));
  assert(!commandAllowed(.25f,.2f,1.f,0,0,true));
  assert(!commandAllowed(.25f,.2f,-5.f,0,0,false));
  assert(!commandAllowed(std::numeric_limits<float>::quiet_NaN(),.2f,-5.f,0,0,true));
  d.reset();assert(d.arm(14370,5.1f));
  assert(!d.observe(14754,14752,.38f,.36f,-6.6f,-6.49f,true,false));
  assert(!d.observe(14754,14754,.36f,.34f,-6.49f,-5.99f,true,false));
  assert(!d.observe(14760,14754,.34f,.33f,-5.99f,-5.82f,true,false));
  assert(!d.observe(14762,14760,.33f,.30f,-5.82f,-5.69f,true,false));
  assert(d.observe(14767,14762,.30f,.27f,-5.69f,-4.96f,true,false));
  d.reset();assert(d.arm(14370,5.1f));
  assert(!d.observe(14754,14752,.38f,.36f,-6.6f,-6.49f,true,false));
  assert(!d.observe(14753,14754,.36f,.35f,-6.49f,-5.9f,true,false));
  assert(!d.observe(14760,14753,.35f,.33f,-5.9f,-5.82f,true,false));
  d.reset();assert(d.arm(14370,5.1f));
  assert(!d.observe(14754,14752,.38f,.36f,-6.6f,-6.49f,true,false));
  assert(!d.observe(14754,14754,.36f,0.f,-6.49f,-5.9f,true,false));
  assert(!d.observe(14760,14754,.36f,.33f,-6.49f,-5.82f,true,false));
  // run305 rounded samples, actual runner gyro-rate scaling: the crossing
  // happens before the 0.35-degree gate, but inside the research 0.40 gate.
  d.reset();assert(d.arm(14370,5.1f));
  assert(!d.observe(14754,14752,.37f,.36f,-6.08f,-5.92f,true,false));
  assert(!d.observe(14760,14754,.36f,.33f,-5.92f,-5.30f,true,false));
  assert(d.observe(14766,14760,.33f,.30f,-5.30f,-4.9f,true,false));
  assert(!d.observe(14770,14766,.30f,.27f,-4.9f,-4.2f,true,false));
  assert(commandAllowed(.35f,.35f,-5.f,0,0,true));
  assert(commandAllowed(.40f,.40f,-5.f,0,0,true));
  assert(!commandAllowed(.401f,.40f,-5.f,0,0,true));
  // 0.40-degree research window: observed run305 rounded gyro sequence.
  d.reset();assert(d.arm(14370,5.1f));
  assert(!d.observe(14754,14752,.37f,.36f,-6.081f,-5.917f,true,false));
  assert(!d.observe(14760,14754,.36f,.33f,-5.917f,-5.299f,true,false));
  assert(d.observe(14762,14760,.33f,.30f,-5.299f,-5.199f,true,false));
  // Above window at crossing: no delayed false candidate.
  d.reset();assert(d.arm(14370,5.1f));
  assert(!d.observe(14754,14752,.43f,.401f,-6.081f,-5.917f,true,false));
  assert(!d.observe(14760,14754,.401f,.33f,-5.917f,-5.299f,true,false));
  assert(!d.observe(14762,14760,.33f,.30f,-5.299f,-5.199f,true,false));
  // Ordinary zero crossing, outward noise, and late samples must not fire.
  d.reset();assert(d.arm(14370,5.1f));
  assert(!d.observe(14754,14752,.37f,.36f,-6.081f,-5.917f,true,false));
  assert(!d.observe(14760,14754,.36f,-.01f,-5.917f,-5.299f,true,false));
  assert(!d.observe(14762,14760,.33f,.30f,-5.299f,-5.199f,true,false));
  d.reset();assert(d.arm(14370,5.1f));
  assert(!d.observe(14754,14752,.37f,.36f,-6.081f,-5.917f,true,false));
  assert(!d.observe(14760,14754,.36f,.38f,-5.917f,1.f,true,false));
  assert(!d.observe(14762,14760,.38f,.30f,1.f,-5.199f,true,false));
  d.reset();assert(d.arm(14370,5.1f));
  assert(!d.observe(15171,15165,.37f,.36f,-6.081f,-5.917f,true,false));
  assert(!d.observe(15177,15171,.36f,.33f,-5.917f,-5.299f,true,false));
}
