#include <cassert>
#include <cmath>
#include <cstdio>
#include <initializer_list>
#include "lateral_energy_control.h"

static bool near(float a, float b, float tolerance) {
  return std::fabs(a-b) <= tolerance;
}

int main() {
  using namespace lateral_energy;
  const Geometry g;
  for (float degrees : {0.f, .25f, 1.f, 2.f, 4.f, 8.f, 10.f, 12.f, 16.f}) {
    const float e=potential(g,degrees);
    assert(std::isfinite(e));
    assert(near(angle(g,e),degrees,.015f));
  }
  CurrentTable table;
  assert(table.prepare(.025f));
  assert(near(charge(table,0,0,300),0,.000001f));
  assert(charge(table,100,0,300)>20.f);
  for (int side : {-1,1}) {
    const auto c=coefficients(side);
    for (float rate : {40.f,60.f,85.f}) {
      const float omega=rate*kRadPerDeg;
      for (float required : {.00001f,.00010f,.00030f}) {
        const float q=requiredCharge(c,omega,required);
        assert(std::isfinite(q) && q>0);
        assert(near(work(c,omega,q),required,1.e-7f));
      }
    }
    const auto d=decide(g,table,side,65, potential(g,10),0,0,300);
    assert(d.valid && d.ms>0 && d.ms<=100);
    assert(d.evaluations<=10);
    assert(d.kinetic_j>0 && d.loss_j>0);
    assert(d.predicted_j>=d.free_j);
    assert(d.selected_q_mAs<=d.available_q_mAs);
    assert(d.energy_error_j<.00003f);
    const auto coast=decide(g,table,side,95,potential(g,8),0,0,300);
    assert(coast.valid && coast.ms==0);
  }
  const auto reverse=decide(g,table,1,65,potential(g,10),0,-150,300);
  assert(reverse.valid && reverse.ms<=100 && reverse.selected_q_mAs>=0);
  assert(near(updateIntegral(0,.0001f,.001f,false,false,true),.00001f,1.e-8f));
  assert(updateIntegral(0,.0001f,.001f,true,false,true)==0);
  assert(!decide(g,table,1,65,potential(g,10),0,0,0).valid);
  std::puts("lateral energy model: OK");
}
