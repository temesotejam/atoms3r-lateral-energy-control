#pragma once
#include "camera_experiment_mode.h"
#include "camera_coexistence.h"
#include "run_control_worker.h"
#include "psram_string.h"
#include "foot_zero.h"
#include "white_marker_tracker.h"

struct FootFrame {
  uint64_t frame_us = 0, delivered_us = 0;
  // Snapshot return time on the control task; zero means not recorded.
  uint64_t control_observed_us = 0;
  int64_t log_time_us = -1, measurement_time_us = -1;
  uint32_t sequence = 0, imu_sample_us = 0, processing_us = 0;
  uint16_t run_id = 0;
  uint8_t state_id = 0, led_state = 0, sync_event_id = 0;
  bool timestamp_valid = false, frame_valid = false, zero_ready = false;
  bool right_valid = false, left_valid = false, right_in_range = false, left_in_range = false;
  float right_x = NAN, left_x = NAN, right_deg = NAN, left_deg = NAN;
  float right_contrast = 0, left_contrast = 0;
  float right_scan_y = NAN, left_scan_y = NAN, right_weight = 0, left_weight = 0;
  MarkerDetectionReason right_reason = MarkerDetectionReason::NoFrame;
  MarkerDetectionReason left_reason = MarkerDetectionReason::NoFrame;
  uint8_t right_templates = 0, left_templates = 0;
  uint8_t right_candidates = 0, left_candidates = 0;
  float right_ambiguity = 0, left_ambiguity = 0;
  FootZeroReason zero_reason = FootZeroReason::Waiting;
};
struct FootSnapshot {
  FootFrame latest;
  bool available = false, zero_ready = false, recording = false, overflow = false;
  uint32_t count = 0, frame_failures = 0, zero_samples = 0;
  uint32_t captured_frames = 0, right_marker_failures = 0, left_marker_failures = 0;
  uint32_t processing_max_us = 0;
#if CAMERA_DIAGNOSTIC_ONLY
  uint32_t fast_copy_max_us = 0, fast_record_skipped = 0, fast_run_frames = 0;
  uint32_t fast_run_received = 0, fast_run_invalid_frame = 0;
  uint32_t fast_run_right_invalid = 0, fast_run_left_invalid = 0;
  uint32_t fast_control_observed = 0;
  uint64_t fast_control_last_us = 0, fast_control_max_gap_us = 0;
  uint64_t fast_run_first_us = 0, fast_run_last_us = 0;
  bool fast_capture_active = false;
#endif
  float right_zero = appcfg::kFootAngleAZeroXPx, left_zero = appcfg::kFootAngleBZeroXPx;
  float fps = 0;
  FootZeroReason zero_reason = FootZeroReason::Waiting;
  bool preview_available = false;
};
struct FootPreviewInfo {
  FootFrame frame;
  MekfAttitudeSnapshot mekf_attitude;
  bool imu_ok = false;
  WhiteMarkerObservation right, left;
  float right_zero = 0, left_zero = 0;
  uint32_t zero_samples = 0;
};
class FootObserver {
 public:
  static constexpr uint32_t kCapacity = 768; // 40 s * 15 Hz, with margin
  static constexpr uint32_t kPreviewWidth = 160, kPreviewHeight = 120;
  static constexpr uint32_t kPreviewBytes = kPreviewWidth * kPreviewHeight;
  bool begin(OneShotCamera& camera, RunControlWorker& control);
  void beginRun(uint16_t id, uint64_t log_epoch_us);
  void finishRun();
  void clearRun();
  FootSnapshot snapshot() const;
  void recordControlObservation(uint16_t run_id, uint32_t sequence, uint64_t observed_us);
  CameraOneShotSnapshot cameraSnapshot() const { return camera_ ? camera_->snapshot() : CameraOneShotSnapshot{}; }
  void appendMetadata(PsramString& json) const; // sealed, export worker only
  bool copyPreview(uint8_t* out, FootPreviewInfo& info) const;
 private:
  static void entry(void* ptr) { static_cast<FootObserver*>(ptr)->loop(); }
  void loop();
  void publishPreview(const uint8_t* gray, const FootPreviewInfo& info, bool run_active);
  OneShotCamera* camera_ = nullptr;
  RunControlWorker* control_ = nullptr;
  TaskHandle_t task_ = nullptr;
  FootFrame* frames_ = nullptr;
  FootZero zero_;
  WhiteMarker1DTracker right_{0}, left_{1};
  mutable portMUX_TYPE mux_ = portMUX_INITIALIZER_UNLOCKED;
  FootSnapshot status_;
  uint16_t recording_run_id_ = 0;
  uint64_t log_epoch_us_ = 0;
  uint8_t* preview_ = nullptr;
  SemaphoreHandle_t preview_mutex_ = nullptr;
  FootPreviewInfo preview_info_;
#if FOOT_FAST_CAPTURE_EXPERIMENT
  uint8_t* fast_frame_copy_ = nullptr;
  bool fast_capture_active_ = false;
#endif
#if CAMERA_DIAGNOSTIC_ONLY
  uint64_t next_recording_frame_us_ = 0;
#endif
};
