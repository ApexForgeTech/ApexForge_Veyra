#ifndef VEYRA_RUNTIME_FINGERPRINT_ENGINE_H_
#define VEYRA_RUNTIME_FINGERPRINT_ENGINE_H_

#include "veyra/models/foundation_models.h"

#include <string>

namespace veyra {

struct FingerprintPolicy {
  std::string profile_id;
  std::string profile_display_name;

  bool inject_canvas_noise = false;
  std::string canvas_noise_level;
  unsigned int canvas_seed = 0;

  bool override_webgl = false;
  std::string webgl_vendor;
  std::string webgl_renderer;

  bool inject_audio_noise = false;
  double audio_noise_magnitude = 0.0;

  bool override_hardware_concurrency = false;
  int hardware_concurrency = 4;

  bool override_device_memory = false;
  int device_memory = 8;

  bool override_platform = false;
  std::string platform;

  bool override_screen = false;
  int screen_width = 1920;
  int screen_height = 1080;
  int color_depth = 24;
  double device_pixel_ratio = 1.0;
};

FingerprintPolicy BuildFingerprintPolicy(const FingerprintProfileDefinition& profile,
                                          const std::string& session_id);

std::string BuildFingerprintScript(const FingerprintPolicy& policy);

}  // namespace veyra

#endif  // VEYRA_RUNTIME_FINGERPRINT_ENGINE_H_
