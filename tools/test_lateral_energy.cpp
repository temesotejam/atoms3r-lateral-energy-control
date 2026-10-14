#include <cassert>
#include <cmath>
#include <cstdio>
#include <initializer_list>
#include "lateral_energy_control.h"
#include "peak_error_adaptation.h"
#include "autonomous_brake_limit.h"

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
  uint16_t selected_brake_ms = 99;
  for (const char* choice : {"0", "20", "50"})
    assert(autonomous_brake_limit::parse(choice, selected_brake_ms));
  for (const char* invalid : {"", "-1", "10", "51", "20ms"})
    assert(!autonomous_brake_limit::parse(invalid, selected_brake_ms));
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
      const float brake_q=requiredSignedCharge(c,omega,-.00002f);
      assert(std::isfinite(brake_q) && brake_q<0);
      assert(near(work(c,omega,brake_q),-.00002f,1.e-7f));
    }
    const auto d=decide(g,table,side,65, potential(g,10),0,0,300);
    assert(d.valid && d.ms>0 && d.ms<=100);
    assert(d.evaluations<=10);
    assert(d.kinetic_j>0 && d.loss_j>0);
    assert(d.predicted_j>=d.free_j);
    assert(d.selected_q_mAs<=d.available_q_mAs);
    assert(d.energy_error_j<.00003f);
    const auto brake=decide(g,table,side,75,potential(g,8),0,0,300);
    assert(brake.valid && brake.ms>0 && brake.ms<=kBrakeMaxMs);
    assert(brake.command_sign==-1 && brake.selected_q_mAs<0);
    assert(brake.predicted_j<brake.free_j && brake.extrapolated);
    assert(brake.evaluations<=10);
    const auto no_brake=decide(g,table,side,75,potential(g,8),0,0,300,0);
    assert(no_brake.valid && no_brake.ms==0 && no_brake.command_sign==0);
    const auto short_brake=decide(g,table,side,75,potential(g,8),0,0,300,20);
    assert(short_brake.valid && short_brake.ms<=20);
    assert(short_brake.predicted_j>=brake.predicted_j);
    const auto coast=decide(g,table,side,60,
        freeEnergy(c,60*kRadPerDeg),0,0,300);
    assert(coast.valid && coast.ms==0 && coast.command_sign==0);
    assert(!coast.lower && !coast.upper);
  }
  const auto reverse=decide(g,table,1,65,potential(g,10),0,-150,300);
  assert(reverse.valid && reverse.ms<=100 && reverse.selected_q_mAs>=0);
  {
    const auto c = coefficients(1);
    const float omega = 60.f * kRadPerDeg;
    const float target = freeEnergy(c, omega) + work(c, omega, 5.f);
    const auto original = decide(g,table,1,60,target,0,0,300);
    const auto corrected = decide(g,table,1,60,target,0,0,300,kBrakeMaxMs,.000025f);
    assert(original.valid && corrected.valid);
    assert(corrected.selected_q_mAs < original.selected_q_mAs);
    assert(corrected.ms < original.ms);
  }
  {
    peak_error_adaptation::Model model;
    uint8_t bin=255, count=255;
    for (int i=0;i<4;++i)
      model.observe(5000+i*500,1,peak_error_adaptation::Excite,50,true,10000,.00004f);
    assert(near(model.correction(1,peak_error_adaptation::Excite,50,true,10000,&bin,&count),
                .00004f,1.e-8f));
    assert(bin==peak_error_adaptation::Slow && count==4);
    assert(model.correction(-1,peak_error_adaptation::Excite,50,true,10000)==0);
    assert(model.correction(1,peak_error_adaptation::Brake,50,true,10000)==0);
    assert(model.correction(1,peak_error_adaptation::Excite,150,true,10000)==0);
    assert(model.correction(1,peak_error_adaptation::Excite,50,true,60000)==0);
    model.observe(7100,1,peak_error_adaptation::Excite,50,true,10000,-.00004f);
    assert(model.correction(1,peak_error_adaptation::Excite,50,true,10000)==0);
    model.reset();
    assert(model.correction(1,peak_error_adaptation::Excite,50,true,10000)==0);
  }
  // Compare the bounded inverse with every legal integer pulse width. The
  // residual current may initially oppose either requested direction.
  for (int side : {-1,1}) {
    const auto c=coefficients(side);
    for (float rate : {18.f,35.f,50.f,65.f,80.f,95.f}) {
      for (float target_deg : {8.f,10.f,12.f}) {
        for (float initial : {-200.f,0.f,200.f}) {
          for (float goal : {300.f,1200.f}) {
            const auto d=decide(g,table,side,rate,potential(g,target_deg),0,initial,goal);
            if (!d.valid || d.command_sign==0) continue;
            const uint16_t max_ms=d.command_sign<0?kBrakeMaxMs:kMaxMs;
            assert(d.ms<=max_ms);
            const float signed_goal=d.command_sign*goal;
            float best=std::fabs(d.corrected_target_j-d.free_j);
            for (uint16_t ms=1;ms<=max_ms;++ms) {
              const float q=charge(table,ms,initial,signed_goal);
              if (d.command_sign<0 ? (q>0 || q< -d.available_q_mAs) : q<0) continue;
              const float predicted=d.free_j+work(c,rate*kRadPerDeg,q);
              if (predicted<0 || predicted>potential(g,16.f)) continue;
              best=std::fmin(best,std::fabs(predicted-d.corrected_target_j));
            }
            assert(d.energy_error_j<=best+1.e-8f);
          }
        }
      }
    }
  }
  assert(near(updateIntegral(0,.0001f,.001f,false,false,true),.00001f,1.e-8f));
  assert(updateIntegral(0,.0001f,.001f,true,false,true)==0);
  assert(!decide(g,table,1,65,potential(g,10),0,0,0).valid);
  assert(!decide(g,table,1,75,potential(g,8),0,0,300,51).valid);
  std::puts("lateral energy model: OK");
}
