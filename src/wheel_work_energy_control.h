#pragma once
#include <cmath>
#include <cstdint>
#include "lateral_energy_control.h"
#include "lateral_cg_energy.h"

// Empirical fixed-leg, one-peak energy models.  Coefficients were fitted to
// complete wheel work (pulse plus coast to the next peak), in mJ.  The work
// multiplier is a regression coefficient, not a mechanical efficiency.
namespace wheel_work_energy_control {
constexpr char kRevision[] = "wheel_work_absolute_side_feedback_20261026";
constexpr float kRadPerDeg = 0.017453292519943295f;
constexpr uint16_t kNegativeMaxMs = 70;
constexpr float kPositiveMinQ = 8.79588f;
constexpr float kPositiveMaxQ = 15.33208f;

struct State {
  State() = default;
  State(float rate, float wheel, float battery, bool fresh, float previous = NAN)
      : rate_dps(rate), wheel_rpm(wheel), battery_mv(battery), wheel_fresh(fresh),
        previous_peak_deg(previous) {}
  float rate_dps = NAN;
  float wheel_rpm = NAN;
  float battery_mv = NAN;
  bool wheel_fresh = false;
  float previous_peak_deg = NAN;
  bool absolute_reference = false;
};
struct Prediction {
  bool valid = false;
  float work_mj = NAN;
  float energy_mj = NAN;
  float free_mj = NAN, on_mj = NAN, after_mj = NAN, change_mj = NAN;
  uint8_t branch = 0;  // 1=positive, 4=pooled negative.
  bool cross_condition = false;
  float support_weight=1.f,applied_bias_mj=0.f;
};
struct Choice {
  bool valid = false;
  bool target_below_reachable = false;
  bool target_above_reachable = false;
  uint16_t allowed_cap_ms=0;
  int16_t current_ma = 0;
  uint16_t width_ms = 0;
  float q_mas = NAN;
  float work_mj = NAN;
  float energy_mj = NAN;
  float minimum_mj = NAN;
  float maximum_mj = NAN;
  uint16_t evaluations = 0;
  float free_mj = NAN, on_mj = NAN, after_mj = NAN, change_mj = NAN;
  uint8_t branch = 0;
  bool cross_condition = false;
  float support_weight=0.f,applied_bias_mj=0.f;
};

inline bool freshFinite(const State& s) {
  return s.wheel_fresh && std::isfinite(s.rate_dps) &&
      std::isfinite(s.wheel_rpm) && std::isfinite(s.battery_mv);
}

inline bool extendedNegativeSupported(const State& s) {
  return freshFinite(s) && s.rate_dps >= 63.9133f && s.rate_dps <= 74.0667f &&
      s.wheel_rpm >= 214.89f && s.wheel_rpm <= 575.79f &&
      s.battery_mv >= 7280.f && s.battery_mv <= 7570.f;
}

// Original 31-run input fit plus one high-voltage coast calibration run.
// 8120 mV includes one telemetry step above the observed 8110 mV maximum.
// High-voltage pulse effects are cross-condition predictions at every width.
inline float fullChargeCorrectionMj(float battery_mv) {
  const float weight=std::fmax(0.f,std::fmin(1.f,(battery_mv-8000.f)/100.f));
  return -.05972251687f*weight;
}

// Marginal ranges are not joint validation of every duration at every state.
// A completed-peak gate limits new strong-pulse combinations.
inline bool broadNegativeSupported(const State& s) {
  return freshFinite(s) && s.rate_dps >= 57.2522f && s.rate_dps <= 76.2778f &&
      s.wheel_rpm >= 4.28f && s.wheel_rpm <= 575.79f &&
      s.battery_mv >= 6210.f && s.battery_mv <= 8120.f &&
      std::isfinite(s.previous_peak_deg) && s.previous_peak_deg >= 5.0288f &&
      s.previous_peak_deg <= 8.56178f;
}

inline bool supported(const State& s, int8_t side) {
  if (!freshFinite(s)) return false;
  if (side > 0) return s.rate_dps >= 48.3821f && s.rate_dps <= 72.7405f &&
      s.wheel_rpm >= -1.f && s.wheel_rpm <= 400.f &&
      s.battery_mv >= 7000.f && s.battery_mv <= 8120.f;
  return side < 0 && broadNegativeSupported(s);
}

// Smooth guard bands are bounded extrapolation, not newly validated domains.
inline float ramp(float value) {
  const float t=std::fmax(0.f,std::fmin(1.f,value));return t*t*(3.f-2.f*t);
}
inline float boxWeight(float value,float low,float high,float low_margin,float high_margin) {
  return ramp((value-low+low_margin)/low_margin)*ramp((high+high_margin-value)/high_margin);
}
inline float positiveWorkWeight(const State& s) {
  if (!freshFinite(s) || s.battery_mv<7000.f || s.battery_mv>8120.f) return 0.f;
  return boxWeight(s.rate_dps,48.3821f,72.7405f,4.f,4.f)*
      boxWeight(s.wheel_rpm,-1.f,400.f,10.f,40.f);
}
inline float negativeBoundaryWeight(const State& s) {
  if (!freshFinite(s) || !std::isfinite(s.previous_peak_deg) ||
      s.battery_mv<6210.f || s.battery_mv>8120.f) return 0.f;
  if (s.absolute_reference)
    return boxWeight(s.rate_dps,57.2522f,76.2778f,2.f,2.f)*
        boxWeight(s.wheel_rpm,-1.f,575.79f,1.f,25.f)*
        boxWeight(s.previous_peak_deg,5.0288f,10.f,.3f,.3f);
  return boxWeight(s.rate_dps,57.2522f,76.2778f,2.f,2.f)*
      boxWeight(s.wheel_rpm,4.28f,575.79f,4.28f,25.f)*
      boxWeight(s.previous_peak_deg,5.0288f,8.56178f,.3f,.3f);
}

inline Prediction positive(const State& s, float q_mas) {
  Prediction p;
  if (!supported(s, 1) || !std::isfinite(q_mas) ||
      q_mas < kPositiveMinQ || q_mas > kPositiveMaxQ) return p;
  p.work_mj = 0.5876964034f + 1.409245548f*q_mas*s.rate_dps/1000.f
      - 0.6875595794f*s.wheel_rpm/500.f
      - 0.0455944221f*(s.battery_mv-7350.f)/100.f;
  const float omega=s.rate_dps*kRadPerDeg;
  p.energy_mj=-0.1072020194f + 0.4497842205f*omega*omega
      + 0.3640046397f*p.work_mj;
  p.valid=std::isfinite(p.energy_mj) && p.energy_mj>=0.f;
  p.branch=1;
  return p;
}

// Whole-pulse plus post-pulse work fit: 8 runs, 180 positive arrivals.
// The body label U_ref(angle) is an energy coordinate, not a new geometry measurement.
inline Prediction positiveWorkRaw(const State& s,float q_mas,float bias_mj=0.f) {
  Prediction p;
  if (!freshFinite(s) || !std::isfinite(q_mas) || q_mas<0.f || q_mas>25.f ||
      !std::isfinite(bias_mj)) return p;
  const float omega=s.rate_dps*kRadPerDeg;
  const float work_zero=.36045438418f-.05256293271f*s.wheel_rpm/500.f
      -.10535792788f*(s.battery_mv-7500.f)/1000.f;
  const float qomega=q_mas*omega;
  p.work_mj=work_zero+.06189960458f*qomega;
  p.on_mj=.09858037463f*qomega;
  p.after_mj=p.work_mj-p.on_mj;
  p.change_mj=p.work_mj-work_zero;
  p.free_mj=.07353697995f+.41423900771f*omega*omega+.22502117220f*work_zero;
  p.energy_mj=p.free_mj+.22502117220f*p.change_mj+bias_mj;
  p.valid=std::isfinite(p.energy_mj) && p.energy_mj>=0.f;
  p.branch=6;p.applied_bias_mj=bias_mj;
  p.cross_condition=q_mas>kPositiveMaxQ || s.rate_dps>58.9332f ||
      s.wheel_rpm<213.22f || s.battery_mv<7280.f || s.battery_mv>7400.f;
  return p;
}

inline Prediction positiveExpanded(const State& s,float q_mas,float bias_mj=0.f) {
  return supported(s,1) ? positiveWorkRaw(s,q_mas,bias_mj) : Prediction{};
}

// Preserve the historical rate/input response as a bounded start-up fallback,
// but allow all three already-tested command currents. Choose in energy units.
inline Prediction positiveBridge(const lateral_energy::Geometry& geometry,
    const State& s,float q_mas,float bias_mj) {
  Prediction p;
  if (!std::isfinite(s.rate_dps) || s.rate_dps<0.f ||
      !std::isfinite(q_mas) || q_mas<0.f || !std::isfinite(bias_mj)) return p;
  p.free_mj=lateral_energy::potential(geometry,lateral_energy::bridgeFreeAngle(1,s.rate_dps))*1000.f;
  p.energy_mj=lateral_energy::bridgeArrivalEnergy(geometry,1,s.rate_dps,q_mas)*1000.f+bias_mj;
  p.valid=std::isfinite(p.energy_mj) && p.energy_mj>=0.f;
  p.branch=5;p.support_weight=0.f;p.applied_bias_mj=bias_mj;
  p.cross_condition=true;
  return p;
}

inline Prediction positiveBoundary(const lateral_energy::Geometry& geometry,const State& s,
    float q_mas,float work_bias,float bridge_bias) {
  const float weight=positiveWorkWeight(s);
  if (weight<=0.f) return positiveBridge(geometry,s,q_mas,bridge_bias);
  auto work=positiveWorkRaw(s,q_mas,work_bias);
  if (weight>=1.f || !work.valid) return work;
  const auto bridge=positiveBridge(geometry,s,q_mas,bridge_bias);
  if (!bridge.valid) return Prediction{};
  Prediction p;p.valid=true;p.branch=7;p.cross_condition=true;p.support_weight=weight;
  p.applied_bias_mj=weight*work_bias+(1.f-weight)*bridge_bias;
  p.energy_mj=weight*work.energy_mj+(1.f-weight)*bridge.energy_mj;
  p.free_mj=weight*work.free_mj+(1.f-weight)*bridge.free_mj;
  // The angle bridge has no measured-work prediction. Do not invent one for a blend.
  return p;
}

inline Prediction negativeRaw(const State& s, uint16_t width_ms, float residual_mj = 0.f) {
  Prediction p;
  if (!freshFinite(s) || (width_ms!=0 && width_ms<10) || width_ms>kNegativeMaxMs ||
      !std::isfinite(residual_mj)) return p;
  const float rate=(s.rate_dps-70.f)/10.f;
  const float rpm=(s.wheel_rpm-200.f)/200.f;
  const float voltage=(s.battery_mv-7300.f)/500.f;
  const float previous=s.previous_peak_deg-8.f;
  const float t=static_cast<float>(width_ms)/25.f;
  p.on_mj=-.18120784069f*t-.11097956656f*t*t-.01345085917f*t*rate
      +.02975585037f*t*rpm-.00233839312f*t*voltage;
  const float after_zero=.26823832254f+.02991961987f*rate+.11699008097f*rpm
      +.00348006791f*voltage;
  p.after_mj=after_zero-.11239169672f*t+.04390893973f*t*t
      +.00789280211f*t*rate-.06387355408f*t*rpm;
  p.work_mj=p.on_mj+p.after_mj;
  p.change_mj=p.work_mj-after_zero;
  const float omega=s.rate_dps*kRadPerDeg;
  p.free_mj=.29017591957f+.68047892000f*omega*omega-.01436825026f*rpm
      +.00500262082f*previous+.01211327167f*voltage+fullChargeCorrectionMj(s.battery_mv);
  p.energy_mj=p.free_mj+.44113787836f*p.change_mj+residual_mj;
  p.valid=std::isfinite(p.energy_mj) && p.energy_mj>=0.f;
  p.branch=4;p.applied_bias_mj=residual_mj;
  p.cross_condition=width_ms>40 || s.battery_mv>8110.f || (width_ms>0 && s.battery_mv>8000.f) ||
      (width_ms>25 && !extendedNegativeSupported(s));
  return p;
}

inline Prediction negative(const State& s,uint16_t width_ms,float residual_mj=0.f) {
  return broadNegativeSupported(s) ? negativeRaw(s,width_ms,residual_mj) : Prediction{};
}
inline Prediction negativeBoundary(const State& s,uint16_t width_ms,float residual_mj=0.f) {
  const float weight=negativeBoundaryWeight(s);
  if (!(weight>0.f)) return Prediction{};
  auto p=negativeRaw(s,width_ms,residual_mj);
  p.support_weight=weight;
  if (weight<1.f) { p.branch=8;p.cross_condition=true; }
  // Absolute-peak context is extended; pulse effects still come from the
  // historical fit and must remain explicitly marked as cross-condition.
  if (s.absolute_reference && !broadNegativeSupported(s)) p.cross_condition=true;
  return p;
}

// Residuals always compare measured peak potential energy to the raw model.
// Instantiate separately for each positive prediction branch. Values are mJ.
class PositiveFeedback {
 public:
  uint8_t count() const { return count_; }
  __attribute__((noinline)) float biasMj() const {
    if (count_<2) return 0.f;
    float sum=0.f;
    for (uint8_t i=0;i<count_;++i) sum+=residuals_[i];
    return std::fmax(-.25f,std::fmin(.25f,sum/count_));
  }
  __attribute__((noinline)) void observe(float actual_mj, float raw_prediction_mj) {
    if (!std::isfinite(actual_mj) || !std::isfinite(raw_prediction_mj) ||
        actual_mj<0.f || raw_prediction_mj<0.f) return;
    residuals_[next_]=actual_mj-raw_prediction_mj;
    next_=(next_+1)%3;
    if (count_<3) ++count_;
  }
 private:
  float residuals_[3]={};
  uint8_t count_=0,next_=0;
};

// Completed negative peaks also gate exploration of longer brake inputs.
class NegativeFeedback {
 public:
  uint16_t capMs(float previous_peak_deg) const {
    return previous_peak_deg>=7.8f ? cap_ms_ : previous_peak_deg>=7.5f ?
        (cap_ms_>40 ? 40 : cap_ms_) : 25;
  }
  uint16_t balancedCapMs(const lateral_energy::Geometry& g,float previous_peak_deg,float cg_mm) const {
    if (!std::isfinite(previous_peak_deg) || previous_peak_deg<7.f) return 0;
    const float floor=lateral_cg::potential(g,7.f,cg_mm);
    const float full=lateral_cg::potential(g,7.8f,cg_mm);
    const float energy=lateral_cg::potential(g,previous_peak_deg,cg_mm);
    if (!std::isfinite(energy) || !(full>floor)) return 0;
    const float reserve=std::fmax(0.f,std::fmin(1.f,(energy-floor)/(full-floor)));
    return static_cast<uint16_t>(std::floor(25.f+reserve*(cap_ms_-25.f)+1e-5f));
  }
  uint8_t count() const { return count_; }
  __attribute__((noinline)) float biasMj() const {
    if (count_<2) return 0.f;
    float sum=0.f;
    for (uint8_t i=0;i<count_;++i) sum+=residuals_[i];
    return std::fmax(-.12f,std::fmin(.12f,sum/count_));
  }
  __attribute__((noinline)) void observe(float actual_mj, float raw_prediction_mj,
      float target_mj, float previous_peak_deg, uint16_t applied_width_ms, bool balanced=false) {
    if (!std::isfinite(actual_mj) || !std::isfinite(raw_prediction_mj) ||
        !std::isfinite(target_mj) || !std::isfinite(previous_peak_deg) ||
        applied_width_ms>kNegativeMaxMs) return;
    if (previous_peak_deg<(balanced ? 7.f : 7.5f)) { cap_ms_=25; high_count_=0; return; }
    if (!balanced && previous_peak_deg<7.8f && cap_ms_>40) { cap_ms_=40; high_count_=0; }
    residuals_[next_]=actual_mj-raw_prediction_mj;
    next_=(next_+1)%3;
    if (count_<3) ++count_;
    const float excess=actual_mj-target_mj;
    if (excess<-.03f) {
      cap_ms_=cap_ms_>25 ? cap_ms_-5 : 25; high_count_=0;
    } else if (excess>.03f && applied_width_ms>=cap_ms_ && cap_ms_<kNegativeMaxMs &&
        (cap_ms_<40 || previous_peak_deg>=7.8f)) {
      if (++high_count_>=2) { cap_ms_+=5; high_count_=0; }
    } else high_count_=0;
  }
 private:
  float residuals_[3] = {};
  uint8_t count_=0, next_=0, high_count_=0;
  uint16_t cap_ms_=25;
};

inline void consider(Choice& c, int16_t current_ma, uint16_t width_ms,
                     float q_mas, const Prediction& p, float target_mj) {
  if (!p.valid) return;
  if (!c.valid) { c.minimum_mj=c.maximum_mj=p.energy_mj; }
  else {
    c.minimum_mj=std::fmin(c.minimum_mj,p.energy_mj);
    c.maximum_mj=std::fmax(c.maximum_mj,p.energy_mj);
  }
  const float error=std::fabs(p.energy_mj-target_mj);
  if (!c.valid || error<std::fabs(c.energy_mj-target_mj) ||
      (error==std::fabs(c.energy_mj-target_mj) && current_ma<c.current_ma)) {
    c.current_ma=current_ma;c.width_ms=width_ms;c.q_mas=q_mas;
    c.work_mj=p.work_mj;c.energy_mj=p.energy_mj;
    c.free_mj=p.free_mj;c.on_mj=p.on_mj;c.after_mj=p.after_mj;
    c.change_mj=p.change_mj;c.branch=p.branch;
    c.cross_condition=p.cross_condition;c.support_weight=p.support_weight;
    c.applied_bias_mj=p.applied_bias_mj;
  }
  c.valid=true;++c.evaluations;
}

inline void finish(Choice& c, float target_mj) {
  if (!c.valid) return;
  c.target_below_reachable=target_mj<c.minimum_mj;
  c.target_above_reachable=target_mj>c.maximum_mj;
}

inline Choice choosePositive(const lateral_energy::Geometry& geometry,const State& state,
    float initial_current_ma,const lateral_energy::CurrentTable* const tables[3],
    const float goals[3],float target_mj,float bias_mj,float bridge_bias_mj=NAN) {
  Choice choice;
  if (!std::isfinite(target_mj) || target_mj<=0.f) return choice;
  if (!std::isfinite(bridge_bias_mj)) bridge_bias_mj=bias_mj;
  const int16_t currents[]={300,350,400};
  for (uint8_t i=0;i<3;++i) {
    for (uint16_t width=0;width<=100;width=(width==0 ? 10 : width+1)) {
      const float q=lateral_energy::charge(*tables[i],width,initial_current_ma,goals[i]);
      const Prediction p=positiveBoundary(geometry,state,q,bias_mj,bridge_bias_mj);
      consider(choice,currents[i],width,q,p,target_mj);
    }
    if (choice.valid && std::fabs(choice.energy_mj-target_mj)<=.005f) break;
  }
  finish(choice,target_mj);
  return choice;
}
inline Choice chooseNegativeBoundary(const State& state,uint16_t cap_ms,float target_mj,float bias_mj) {
  Choice c;const float weight=negativeBoundaryWeight(state);
  if (!(weight>0.f) || !std::isfinite(target_mj) || target_mj<=0.f) return c;
  c.allowed_cap_ms=static_cast<uint16_t>(std::floor(std::fmin<float>(kNegativeMaxMs,cap_ms)*weight+1e-5f));
  for(uint16_t ms=0;ms<=c.allowed_cap_ms;ms=ms==0 ? 10 : ms+1)
    consider(c,300,ms,NAN,negativeBoundary(state,ms,bias_mj),target_mj);
  finish(c,target_mj);return c;
}
}  // namespace wheel_work_energy_control
