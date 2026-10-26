#pragma once
#include "lateral_cg_energy.h"
namespace wheel_work_energy_control {
// Store U0(observed)-Eraw and its signed CG coefficient. Re-express the
// residual using the CURRENT CG estimate, so a paired CG update is not added
// a second time to an already-corrected model prediction. All values are mJ.
class SideEnergyFeedback {
 public:
  unsigned count() const { return count_; }
  __attribute__((noinline)) bool observe(const lateral_energy::Geometry& g, int side,
      float amplitude_deg, float raw_mj) {
    if ((side!=1 && side!=-1) || !std::isfinite(amplitude_deg) ||
        amplitude_deg<6.f || amplitude_deg>13.f || !std::isfinite(raw_mj) || raw_mj<0.f) return false;
    const float residual=1000.f*lateral_energy::potential(g,amplitude_deg)-raw_mj;
    if (!std::isfinite(residual) || std::fabs(residual)>.5f) return false;
    residual_[next_]=residual;
    coefficient_[next_]=g.mass_g*std::sin(side*amplitude_deg*lateral_energy::kRadPerDeg);
    next_=(next_+1)%3;if(count_<3) ++count_;
    return true;
  }
  __attribute__((noinline)) float biasMj(float cg_mm,float fallback_mj) const {
    if (count_<2 || !std::isfinite(cg_mm)) return fallback_mj;
    float sum=0;
    for(unsigned i=0;i<count_;++i) sum+=residual_[i]+coefficient_[i]*cg_mm;
    return std::fmax(-.5f,std::fmin(.5f,sum/count_));
  }
 private:
  float residual_[3]={},coefficient_[3]={};
  unsigned count_=0,next_=0;
};
}
