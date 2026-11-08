#pragma once
#ifndef FOOT_FAST_CAPTURE_EXPERIMENT
#define FOOT_FAST_CAPTURE_EXPERIMENT 0
#endif
#if FOOT_FAST_CAPTURE_EXPERIMENT < 0 || FOOT_FAST_CAPTURE_EXPERIMENT > 2
#error Invalid FOOT_FAST_CAPTURE_EXPERIMENT
#endif
#ifndef CAMERA_DIAGNOSTIC_ONLY
#define CAMERA_DIAGNOSTIC_ONLY (FOOT_FAST_CAPTURE_EXPERIMENT != 0)
#endif
#if FOOT_FAST_CAPTURE_EXPERIMENT && !CAMERA_DIAGNOSTIC_ONLY
#error Fast capture requires passive diagnostic mode
#endif
constexpr bool cameraExperimentAllowsCurrent(int current_ma) {
#if CAMERA_DIAGNOSTIC_ONLY
  return current_ma == 0;
#else
  (void)current_ma;
  return true;
#endif
}

#include <stdio.h>
#ifndef CAMERA_BUILD_SHA_0
#define CAMERA_BUILD_SHA_0 0UL
#define CAMERA_BUILD_SHA_1 0UL
#define CAMERA_BUILD_SHA_2 0UL
#define CAMERA_BUILD_SHA_3 0UL
#define CAMERA_BUILD_SHA_4 0UL
#endif
inline void cameraBuildSha(char (&out)[41]) {
  if (!(CAMERA_BUILD_SHA_0 | CAMERA_BUILD_SHA_1 | CAMERA_BUILD_SHA_2 | CAMERA_BUILD_SHA_3 | CAMERA_BUILD_SHA_4)) {
    snprintf(out, sizeof(out), "unknown");
  } else {
    snprintf(out, sizeof(out), "%08lx%08lx%08lx%08lx%08lx",
        static_cast<unsigned long>(CAMERA_BUILD_SHA_0), static_cast<unsigned long>(CAMERA_BUILD_SHA_1),
        static_cast<unsigned long>(CAMERA_BUILD_SHA_2), static_cast<unsigned long>(CAMERA_BUILD_SHA_3),
        static_cast<unsigned long>(CAMERA_BUILD_SHA_4));
  }
}
