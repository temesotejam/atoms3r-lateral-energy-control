#pragma once
#include <cmath>
#include <cstdint>

// Empirical fixed-leg, one-peak energy models.  Coefficients were fitted to
// complete wheel work (pulse plus coast to the next peak), in mJ.  The work
// multiplier is a regression coefficient, not a mechanical efficiency.
namespace wheel_work_energy_control {
constexpr char kRevision[] = "wheel_work_broad_negative_19runs_20261023";
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
  uint8_t branch = 0;  // 1=positive, 2=extended negative, 3=broad negative.
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

// Joint refit of the 5 low-voltage and 14 later randomized 0/10/15/20/25-ms
// runs (412 events). These are observed marginal state ranges, not proof that
// every combination has independent data. Never extend the pulse past 25 ms.
inline bool broadNegativeSupported(const State& s) {
  return freshFinite(s) && s.rate_dps >= 57.2522f && s.rate_dps <= 76.2778f &&
      s.wheel_rpm >= 4.28f && s.wheel_rpm <= 550.04f &&
      s.battery_mv >= 6210.f && s.battery_mv <= 8000.f &&
      std::isfinite(s.previous_peak_deg) && s.previous_peak_deg >= 5.0288f &&
      s.previous_peak_deg <= 8.56178f;
}

inline bool supported(const State& s, int8_t side) {
  if (!freshFinite(s)) return false;
  if (side > 0) return s.rate_dps >= 48.3821f && s.rate_dps <= 58.9332f &&
      s.wheel_rpm >= 213.22f && s.wheel_rpm <= 394.61f &&
      s.battery_mv >= 7280.f && s.battery_mv <= 7400.f;
  return side < 0 && (extendedNegativeSupported(s) || broadNegativeSupported(s));
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

inline Prediction negative(const State& s, uint16_t width_ms) {
  Prediction p;
  if (!extendedNegativeSupported(s)) {
    if (!broadNegativeSupported(s) || (width_ms!=0 && width_ms!=10 && width_ms!=15 &&
        width_ms!=20 && width_ms!=25)) return p;
    const float rate=(s.rate_dps-72.5f)/3.f;
    const float rpm=(s.wheel_rpm-80.f)/50.f;
    const float previous=(s.previous_peak_deg-8.f)/.5f;
    const float voltage=(s.battery_mv-7000.f)/1000.f;
    const float t=static_cast<float>(width_ms)/25.f;
    p.on_mj=-.11329463358f*t-.21521292582f*t*t
        -.01046911708f*t*rate+.00317603809f*t*rpm;
    const float after_zero=.22687838001f+.00773765566f*rate+.01946049744f*rpm;
    p.after_mj=after_zero-.13005020970f*t+.08070477472f*t*t;
    p.work_mj=p.on_mj+p.after_mj;
    p.change_mj=p.work_mj-after_zero;
    p.free_mj=1.39289948600f+.09102083655f*rate-.02417967090f*rpm
        -.04131838521f*previous+.01920426738f*voltage;
    p.energy_mj=p.free_mj+.42577061840f*p.change_mj;
    p.valid=std::isfinite(p.energy_mj) && p.energy_mj>=0.f;
    p.branch=3;
    return p;
  }
  if (width_ms!=30 && width_ms!=35 && width_ms!=40) return p;
  p.work_mj=-0.3599415447f - 0.1670413735f*(static_cast<float>(width_ms)-35.f)/10.f
      + 0.0091344913f*(s.rate_dps-65.f)/20.f
      + 0.1618294320f*s.wheel_rpm/500.f
      + 0.0025971679f*(s.battery_mv-7400.f)/100.f;
  const float omega=s.rate_dps*kRadPerDeg;
  p.energy_mj=0.2486528426f + 0.5325578488f*omega*omega
      + 0.2257086551f*p.work_mj;
  p.valid=std::isfinite(p.energy_mj) && p.energy_mj>=0.f;
  p.branch=2;
  return p;
}

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
  }
  c.valid=true;++c.evaluations;
}

inline void finish(Choice& c, float target_mj) {
  if (!c.valid) return;
  c.target_below_reachable=target_mj<c.minimum_mj;
  c.target_above_reachable=target_mj>c.maximum_mj;
}
}  // namespace wheel_work_energy_control
