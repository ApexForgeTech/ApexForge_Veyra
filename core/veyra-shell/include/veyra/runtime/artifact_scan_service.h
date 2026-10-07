#ifndef VEYRA_RUNTIME_ARTIFACT_SCAN_SERVICE_H_
#define VEYRA_RUNTIME_ARTIFACT_SCAN_SERVICE_H_

#include <cstdint>
#include <string>
#include <vector>

namespace veyra {

struct ArtifactScanRequest {
  std::string artifact_id;
  std::string quarantine_path;
  std::string source_url;
  std::string suggested_filename;
};

struct ArtifactScanReport {
  std::string artifact_id;
  std::string quarantine_path;
  std::string source_url;
  std::string suggested_filename;
  std::string detected_name;
  std::uint64_t size_bytes = 0;
  std::string mime_guess;
  std::string sha256;
  std::string sha1;
  int risk_score = 0;
  std::string risk_level;
  std::vector<std::string> heuristics;
  std::string summary;
};

class ArtifactScanClient {
 public:
  explicit ArtifactScanClient(std::string binary_path);

  bool Scan(const ArtifactScanRequest& request,
            ArtifactScanReport* report,
            std::string* error) const;

 private:
  std::string binary_path_;
};

}  // namespace veyra

#endif  // VEYRA_RUNTIME_ARTIFACT_SCAN_SERVICE_H_
