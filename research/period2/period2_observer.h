#pragma once
#include <cmath>
#include <cstdint>

// Observation-only. It never requests or changes a motor command.
// A completed pair consists of a candidate-strong-side peak and the next
// opposite-side peak, each paired with its own preceding decision.
namespace period2_observer {
enum class Outcome : uint8_t { Missing=0, Sent=1, IntentionalCoast=2 };
struct Pair {
  uint32_t strong_decision_ms=0,strong_peak_ms=0,weak_decision_ms=0,weak_peak_ms=0;
  int8_t strong_side=0;
  float stage_target_deg=NAN,strong_deg=NAN,weak_deg=NAN;
  Outcome strong_outcome=Outcome::Missing,weak_outcome=Outcome::Missing;
};
struct Detection {
  bool valid=false;
  // A descriptive bound, not authorization or evidence of intervention benefit.
  bool within_stage_plus_2p5=false;
  int8_t strong_side=0; uint32_t end_ms=0;
};
class Observer {
 public:
  void reset(){*this=Observer{};}
  Detection observe(const Pair& p,uint32_t now) {
    Detection result;
    // All data must be available in order at call time. No future values,
    // repeated peaks, backwards clock or unknown send/coast outcome.
    if((p.strong_side!=1 && p.strong_side!=-1) || !p.strong_decision_ms ||
       p.strong_decision_ms>=p.strong_peak_ms || p.strong_peak_ms>=p.weak_decision_ms ||
       p.weak_decision_ms>=p.weak_peak_ms || p.weak_peak_ms>now || now-p.weak_peak_ms>1000 ||
       p.weak_peak_ms-p.strong_decision_ms>1500 ||
       (count_ && (now<last_now_ || p.strong_decision_ms<=pairs_[count_-1].weak_peak_ms)) ||
       !std::isfinite(p.stage_target_deg) || p.stage_target_deg<2.f || p.stage_target_deg>15.f ||
       !std::isfinite(p.strong_deg) || !std::isfinite(p.weak_deg) ||
       p.strong_deg<0.f || p.strong_deg>20.f || p.weak_deg<0.f || p.weak_deg>20.f ||
       (p.strong_outcome!=Outcome::Sent && p.strong_outcome!=Outcome::IntentionalCoast) ||
       (p.weak_outcome!=Outcome::Sent && p.weak_outcome!=Outcome::IntentionalCoast)) { reset(); return result; }
    if(count_ && (p.strong_side!=pairs_[0].strong_side ||
                  std::fabs(p.stage_target_deg-pairs_[0].stage_target_deg)>0.05f)) reset();
    if(count_==4){for(int i=0;i<3;++i)pairs_[i]=pairs_[i+1];count_=3;}
    pairs_[count_++]=p;last_now_=now;
    if(count_<4)return result;
    if(now-pairs_[0].weak_peak_ms>6000){reset();return result;}
    // Anchored phase: high on pair 0 and 2. A shifted window is evaluated
    // automatically on the following completed pair.
    for(int i=0;i<4;++i){
      const bool high=pairs_[i].strong_deg>pairs_[i].stage_target_deg+1.5f;
      const bool should_high=(i%2)==0;
      if(high!=should_high)return result;
      if(should_high) {
        if(pairs_[i].weak_outcome!=Outcome::IntentionalCoast ||
           pairs_[i].weak_deg>pairs_[i].stage_target_deg-0.5f)return result;
      } else if(pairs_[i].weak_outcome!=Outcome::Sent)return result;
    }
    result.valid=true;result.within_stage_plus_2p5=true;
    for(int i=0;i<4;++i)if(pairs_[i].strong_deg>pairs_[i].stage_target_deg+2.5f)result.within_stage_plus_2p5=false;
    result.strong_side=p.strong_side;result.end_ms=p.weak_peak_ms;return result;
  }
 private:
  Pair pairs_[4];uint8_t count_=0;uint32_t last_now_=0;
};
}
