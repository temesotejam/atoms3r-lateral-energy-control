#pragma once
#include <cmath>
#include <cstdint>

// Empirical fixed-leg, one-peak energy models.  Coefficients were fitted to
// complete wheel work (pulse plus coast to the next peak), in mJ.  The work
// multiplier is a regression coefficient, not a mechanical efficiency.
namespace wheel_work_energy_control {
constexpr char kRevision[] = "wheel_work_pooled_negative_31runs_20261023";
constexpr float kRadPerDeg = 0.017453292519943295f;
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
};
struct Prediction {
  bool valid = false;
  float work_mj = NAN;
  float energy_mj = NAN;
  float free_mj = NAN, on_mj = NAN, after_mj = NAN, change_mj = NAN;
  uint8_t branch = 0;  // 1=positive, 4=pooled negative.
  bool cross_condition = false;
};
struct Choice {
  bool valid = false;
  bool target_below_reachable = false;
  bool target_above_reachable = false;
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

// Marginal ranges of the 31-run fit; not joint validation of every duration
// at every state. A completed-peak gate limits new strong-pulse combinations.
inline bool broadNegativeSupported(const State& s) {
  return freshFinite(s) && s.rate_dps >= 57.2522f && s.rate_dps <= 76.2778f &&
      s.wheel_rpm >= 4.28f && s.wheel_rpm <= 575.79f &&
      s.battery_mv >= 6210.f && s.battery_mv <= 8000.f &&
      std::isfinite(s.previous_peak_deg) && s.previous_peak_deg >= 5.0288f &&
      s.previous_peak_deg <= 8.56178f;
}

inline bool supported(const State& s, int8_t side) {
  if (!freshFinite(s)) return false;
  if (side > 0) return s.rate_dps >= 48.3821f && s.rate_dps <= 58.9332f &&
      s.wheel_rpm >= 213.22f && s.wheel_rpm <= 394.61f &&
      s.battery_mv >= 7280.f && s.battery_mv <= 7400.f;
  return side < 0 && broadNegativeSupported(s);
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

inline Prediction negative(const State& s, uint16_t width_ms, float residual_mj = 0.f) {
  Prediction p;
  if (!broadNegativeSupported(s) || (width_ms!=0 && width_ms<10) || width_ms>40 ||
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
      +.00500262082f*previous+.01211327167f*voltage;
  p.energy_mj=p.free_mj+.44113787836f*p.change_mj+residual_mj;
  p.valid=std::isfinite(p.energy_mj) && p.energy_mj>=0.f;
  p.branch=4;
  p.cross_condition=width_ms>25 && !extendedNegativeSupported(s);
  return p;
}

// Energy-valued feedback from completed, matched negative peaks. The input
// duration still minimizes the energy error; this cap only controls how fast
// a new strong-pulse operating combination may be attempted.
class NegativeFeedback {
 public:
  uint16_t capMs(float previous_peak_deg) const {
    return previous_peak_deg>=7.5f ? cap_ms_ : 25;
  }
  uint8_t count() const { return count_; }
  __attribute__((noinline)) float biasMj() const {
    if (count_<2) return 0.f;
    float sum=0.f;
    for (uint8_t i=0;i<count_;++i) sum+=residuals_[i];
    return std::fmax(-.12f,std::fmin(.12f,sum/count_));
  }
  __attribute__((noinline)) void observe(float actual_mj, float raw_prediction_mj,
      float target_mj, float previous_peak_deg, uint16_t applied_width_ms) {
    if (!std::isfinite(actual_mj) || !std::isfinite(raw_prediction_mj) ||
        !std::isfinite(target_mj) || !std::isfinite(previous_peak_deg) ||
        applied_width_ms>40) return;
    if (previous_peak_deg<7.5f) { cap_ms_=25; high_count_=0; return; }
    residuals_[next_]=actual_mj-raw_prediction_mj;
    next_=(next_+1)%3;
    if (count_<3) ++count_;
    const float excess=actual_mj-target_mj;
    if (excess<-.03f) {
      cap_ms_=cap_ms_>25 ? cap_ms_-5 : 25; high_count_=0;
    } else if (excess>.03f && applied_width_ms>=cap_ms_ && cap_ms_<40) {
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
    c.cross_condition=p.cross_condition;
  }
  c.valid=true;++c.evaluations;
}

inline void finish(Choice& c, float target_mj) {
  if (!c.valid) return;
  c.target_below_reachable=target_mj<c.minimum_mj;
  c.target_above_reachable=target_mj>c.maximum_mj;
}
}  // namespace wheel_work_energy_control
