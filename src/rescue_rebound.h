#pragma once
#include <cmath>
#include <cstdint>
#ifndef RESEARCH_RESCUE_REBOUND
#define RESEARCH_RESCUE_REBOUND 0
#endif

// Experimental state-only recovery after a successful inward stall pulse.
// A same-side peak is real but is NOT evidence that the requested opposite
// peak occurred or that an extra motor input will improve the gait.
namespace rescue_rebound {
struct Peak { bool valid=false; uint32_t ms=0; float angle=0,detector_angle=0; };
class Detector {
 public:
  void reset() { *this=Detector{}; }
  bool suppressRescue() const { return used_; }
  void normalCrossing() { reset(); }
  // A candidate is not consumed until the runner actually accepts its peak.
  // Rejection starts a new outward/return observation, never repeats the same
  // stale extremum and never marks the one allowed rearm as used.
  void resolve(bool accepted,uint32_t now,float detector_angle) {
    if(!candidate_pending_)return;
    candidate_pending_=false;
    if(accepted){used_=true;armed_=false;return;}
    if(!std::isfinite(detector_angle) || detector_angle*side_<=0 || now<last_ || now-start_>1500) {
      armed_=false;return;
    }
    last_=now;outward_=false;minimum_=detector_angle*side_;
    maximum_=0;peak_measured_=0;peak_ms_=return_since_=0;
  }
  void arm(uint32_t now, int8_t side) {
    if (used_ || !now || side!=1) return;
    armed_=true;outward_=false;candidate_pending_=false;start_=now;last_=now;side_=side;minimum_=INFINITY;
    maximum_=0;peak_measured_=0;peak_ms_=0;return_since_=0;
  }
  Peak observe(uint32_t now,float angle,float measured_angle,float rate,bool eligible) {
    Peak out;
    if (!armed_) return out;
    if (candidate_pending_) return out;
    if (now==last_) return out; // repeated millisecond; wait for a new sample
    if (!eligible || !std::isfinite(angle) || !std::isfinite(measured_angle) || !std::isfinite(rate) ||
        now<last_ || now-last_>20 || now-start_>1500 || angle*side_<=0 || measured_angle*side_<=0 ||
        std::fabs(angle-measured_angle)>0.5f) {
      armed_=false; return out;
    }
    last_=now;
    const float a=angle*side_, v=rate*side_;
    if(!outward_) {
      if(a<minimum_) minimum_=a;
      if(now-start_>=80 && a>=minimum_+0.6f && v>=2.0f) {
        outward_=true; maximum_=a; peak_measured_=measured_angle;peak_ms_=now;
      }
      return out;
    }
    if(a>maximum_) {maximum_=a;peak_measured_=measured_angle;peak_ms_=now;return_since_=0;}
    if(a>=2.0f && maximum_>=minimum_+1.0f && maximum_-a>=0.25f && v<=-2.f) {
      if(!return_since_)return_since_=now;
      else if(now-return_since_>=6) {
        out.valid=true;out.ms=peak_ms_;out.angle=peak_measured_;out.detector_angle=side_*maximum_;
        candidate_pending_=true;
      }
    } else return_since_=0;
    return out;
  }
 private:
  bool armed_=false,used_=false,outward_=false,candidate_pending_=false;
  uint32_t start_=0,last_=0,peak_ms_=0,return_since_=0;
  int8_t side_=0;float minimum_=INFINITY,maximum_=0,peak_measured_=0;
};
}
