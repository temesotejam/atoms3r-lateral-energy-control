#pragma once
#include <cmath>
#include <cstdint>
#ifndef RESEARCH_FOLLOWUP_RESCUE
#define RESEARCH_FOLLOWUP_RESCUE 0
#endif
#if RESEARCH_FOLLOWUP_RESCUE && !RESEARCH_RESCUE_REBOUND
#error Followup rescue requires rebound research
#endif

// Research detector only. It cannot issue a motor command. It proposes one
// bounded retry on the positive-side approach after an independently validated
// same-side rebound; actual ZERO/pulse/health gates remain the caller's duty.
namespace followup_stall {
// Checked again immediately before asking the motor layer to write. This is
// a software guard on two sampled coordinates, not proof of physical contact.
inline bool commandAllowed(float detector,float measured,float rate,uint32_t age_us,
                           uint32_t decision_age_ms,bool healthy) {
  return healthy && std::isfinite(detector) && std::isfinite(measured) && std::isfinite(rate) &&
      detector>=0.15f && detector<=0.40f && measured>0.f &&
      std::fabs(detector-measured)<=0.5f && rate>=-6.5f && rate < -0.1f &&
      age_us<=5000 && decision_age_ms<=5;
}
class Detector {
 public:
  void reset() { *this = Detector{}; }
  void cancel() { armed_=false;pending_ms_=0;samples_=0; }
  bool arm(uint32_t peak_ms, float peak_deg) {
    if (used_ || !peak_ms || !std::isfinite(peak_deg) || peak_deg < 5.f) return false;
    peak_ms_=peak_ms; pending_ms_=0; samples_=0; armed_=true; sample_seen_=false; return true;
  }
  bool observe(uint32_t now,uint32_t previous_ms,float previous_angle,float angle,
               float previous_rate,float rate,bool healthy,bool pulse_active) {
    if(!armed_ || used_) return false;
    if(!healthy || pulse_active || !std::isfinite(previous_angle) || !std::isfinite(angle) ||
       !std::isfinite(previous_rate) || !std::isfinite(rate) || now<=previous_ms ||
       now<peak_ms_ || now-peak_ms_>800 || angle<=0.f) {
      if(healthy && !pulse_active && std::isfinite(previous_angle) && std::isfinite(angle) &&
         std::isfinite(previous_rate) && std::isfinite(rate) && now==previous_ms &&
         now>=peak_ms_ && now-peak_ms_<=800 && angle>0.f) return false;
      armed_=false; pending_ms_=0; return false;
    }
    if(sample_seen_) {
      if(now==last_ms_) return false;
      if(now<last_ms_ || now-last_ms_>10) {cancel();return false;}
      previous_ms=last_ms_;previous_angle=last_angle_;previous_rate=last_rate_;
    } else if(now-previous_ms>10) {cancel();return false;}
    last_ms_=now;last_angle_=angle;last_rate_=rate;sample_seen_=true;
    if(now-peak_ms_<200 || angle<0.15f || angle>0.40f || previous_angle+0.03f<angle ||
       rate < -6.5f || rate >= -0.1f) {pending_ms_=0;samples_=0;return false;}
    if(!pending_ms_) {
      if(previous_rate>=-6.f || rate < -6.f) return false;
      pending_ms_=now;samples_=1;return false;
    }
    if(now-pending_ms_>30) {pending_ms_=0;samples_=0;return false;}
    if(++samples_>=3 && now-pending_ms_>=6) {used_=true;armed_=false;return true;}
    return false;
  }
 private:
  uint32_t peak_ms_=0,pending_ms_=0,last_ms_=0;float last_angle_=0,last_rate_=0;
  unsigned samples_=0;bool armed_=false,used_=false,sample_seen_=false;
};
}
