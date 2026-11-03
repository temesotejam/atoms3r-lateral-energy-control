#pragma once
#include <cmath>
#include <cstdint>

// A missed return crossing is not a model-coverage fault or an emergency.
// This detector proposes at most one recovery per confirmed body peak.
// It does not model contacts or predict the physical outcome of the pulse.
namespace stalled_return {
__attribute__((noinline)) inline bool reacquireDue(bool awaiting,bool pending,bool waiting_peak,uint32_t now,uint32_t command_ms) {
  return awaiting && pending && waiting_peak && now>=command_ms && now-command_ms>1200;
}
class Detector {
 public:
  void reset(){*this=Detector{};}
  // Cancel a partial dwell without allowing a second attempt for a peak.
  void interrupt(){since_=0;last_=0;}
  __attribute__((noinline)) bool update(uint32_t now,uint32_t peak_ms,float peak_abs,
      int side,float angle,float rate,float threshold,bool enabled) {
    if(last_ && (now<=last_ || now-last_>15))since_=0;
    last_=now;
    const bool valid=enabled && now && peak_ms && now>=peak_ms &&
        now-peak_ms>=250 && now-peak_ms<=1400 && peak_ms!=fired_peak_ &&
        (side==1 || side==-1) && std::isfinite(peak_abs) && peak_abs>=4.f &&
        std::isfinite(angle) && std::isfinite(rate) && std::isfinite(threshold);
    const float inward=-side*rate, position=side*angle, distance=side*(angle-threshold);
    // The walker must still be returning, on the same side, and slowed for a
    // sustained interval. Wait when a normal crossing is expected within
    // 50ms at the current speed; the actual future motion remains unknown.
    if(!valid || position<=0.f || position>4.f || distance<=0.f ||
       inward<1.f || inward>10.f || distance/inward<=.05f) {since_=0;return false;}
    if(!since_ || now<since_) {since_=now;return false;}
    if(now-since_<30)return false;
    since_=0;fired_peak_=peak_ms;return true;
  }
 private:
  uint32_t since_=0,fired_peak_=0,last_=0;
};
}
