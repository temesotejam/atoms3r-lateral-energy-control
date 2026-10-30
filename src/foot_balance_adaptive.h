#pragma once
#include <cmath>
#include <cstdint>

// A completed, timestamp-matched half-cycle provides one calibrated marker
// excursion for the corresponding swing foot. Values are relative to zero.
namespace foot_balance_adaptive {
class Controller {
 public:
  void reset(uint16_t run_id) { *this=Controller{};run_id_=run_id; }
  bool observe(int body_side,float foot_excursion,bool valid,uint16_t run_id,
               uint32_t peak_ms,uint32_t frame_ms) {
    if(run_id!=run_id_ || !valid || (body_side!=1 && body_side!=-1) ||
       !std::isfinite(foot_excursion) || foot_excursion<0.f ||
       foot_excursion>25.f || (peak_ms>frame_ms ? peak_ms-frame_ms : frame_ms-peak_ms)>100) return false;
    if(have_last_ && peak_ms<=last_peak_ms_) return false;
    const unsigned side=body_side>0 ? 0u : 1u;
    values_[side][next_[side]]=foot_excursion;times_[side][next_[side]]=peak_ms;
    next_[side]=(next_[side]+1)%3;if(count_[side]<3)++count_[side];
    last_peak_ms_=peak_ms;have_last_=true;
    if(count_[0]<3 || count_[1]<3 || body_side!= -1 ||
       (have_update_ && peak_ms-last_update_ms_<2000)) return true;
    for(unsigned s=0;s<2;++s)for(unsigned i=0;i<3;++i)
      if(peak_ms-times_[s][i]>5000) return true;
    const float left=(values_[0][0]+values_[0][1]+values_[0][2])/3.f;
    const float right=(values_[1][0]+values_[1][1]+values_[1][2])/3.f;
    const float error=left-right;
    if(error>.3f) {
      positive_offset_=std::fmax(0.f,positive_offset_-.1f);
      negative_offset_=std::fmin(1.f,negative_offset_+.1f);
    } else if(error<-.3f) {
      negative_offset_=std::fmax(0.f,negative_offset_-.1f);
      positive_offset_=std::fmin(1.f,positive_offset_+.1f);
    }
    last_update_ms_=peak_ms;have_update_=true;
    return true;
  }
  float negativeOffsetDeg() const { return negative_offset_; }
  float positiveOffsetDeg() const { return positive_offset_; }
  float leftMean() const { return count_[0]==3 ? (values_[0][0]+values_[0][1]+values_[0][2])/3.f : NAN; }
  float rightMean() const { return count_[1]==3 ? (values_[1][0]+values_[1][1]+values_[1][2])/3.f : NAN; }
 private:
  uint16_t run_id_=0;
  float values_[2][3]={{0,0,0},{0,0,0}},negative_offset_=0,positive_offset_=0;
  uint32_t times_[2][3]={{0,0,0},{0,0,0}};
  unsigned next_[2]={},count_[2]={};
  uint32_t last_peak_ms_=0,last_update_ms_=0;
  bool have_last_=false,have_update_=false;
};
}
