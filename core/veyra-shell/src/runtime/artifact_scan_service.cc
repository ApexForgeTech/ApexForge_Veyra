#include "veyra/runtime/artifact_scan_service.h"

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace veyra {
namespace {

std::string SanitizeProtocolField(const std::string& value) {
  std::string sanitized = value;
  for (char& ch : sanitized) {
    if (ch == '\t' || ch == '\n' || ch == '\r') {
      ch = ' ';
    }
  }
  return sanitized;
}

std::vector<std::string> Split(const std::string& input, char delimiter) {
  std::vector<std::string> tokens;
  std::string token;
  std::istringstream stream(input);
  while (std::getline(stream, token, delimiter)) {
    tokens.push_back(token);
  }
  return tokens;
}

void CloseFd(int* fd) {
  if (fd != nullptr && *fd >= 0) {
    close(*fd);
    *fd = -1;
  }
}

bool ReadLine(int fd, std::string* line, std::string* error) {
  line->clear();
  char character = '\0';
  while (true) {
    const ssize_t bytes_read = read(fd, &character, 1);
    if (bytes_read < 0) {
      if (error != nullptr) {
        *error = "Failed to read from artifact scan service: " + std::string(std::strerror(errno));
      }
      return false;
    }
    if (bytes_read == 0) {
      if (error != nullptr) {
        *error = "Artifact scan service closed unexpectedly.";
      }
      return false;
    }
    if (character == '\n') {
      return true;
    }
    line->push_back(character);
  }
}

bool WriteLine(int fd, const std::string& line, std::string* error) {
  const std::string payload = line + "\n";
  const char* cursor = payload.c_str();
  std::size_t remaining = payload.size();
  while (remaining > 0) {
    const ssize_t written = write(fd, cursor, remaining);
    if (written < 0) {
      if (error != nullptr) {
        *error = "Failed to write to artifact scan service: " + std::string(std::strerror(errno));
      }
      return false;
    }
    cursor += written;
    remaining -= static_cast<std::size_t>(written);
  }
  return true;
}

}  // namespace

ArtifactScanClient::ArtifactScanClient(std::string binary_path)
    : binary_path_(std::move(binary_path)) {}

bool ArtifactScanClient::Scan(const ArtifactScanRequest& request,
                              ArtifactScanReport* report,
                              std::string* error) const {
  if (report == nullptr) {
    if (error != nullptr) {
      *error = "Artifact scan report output target was null.";
    }
    return false;
  }

  int stdin_pipe[2] = {-1, -1};
  int stdout_pipe[2] = {-1, -1};
  if (pipe(stdin_pipe) != 0 || pipe(stdout_pipe) != 0) {
    if (error != nullptr) {
      *error = "Failed to create artifact scan pipes: " + std::string(std::strerror(errno));
    }
    CloseFd(&stdin_pipe[0]);
    CloseFd(&stdin_pipe[1]);
    CloseFd(&stdout_pipe[0]);
    CloseFd(&stdout_pipe[1]);
    return false;
  }

  const pid_t child_pid = fork();
  if (child_pid < 0) {
    if (error != nullptr) {
      *error = "Failed to fork artifact scan service: " + std::string(std::strerror(errno));
    }
    CloseFd(&stdin_pipe[0]);
    CloseFd(&stdin_pipe[1]);
    CloseFd(&stdout_pipe[0]);
    CloseFd(&stdout_pipe[1]);
    return false;
  }

  if (child_pid == 0) {
    dup2(stdin_pipe[0], STDIN_FILENO);
    dup2(stdout_pipe[1], STDOUT_FILENO);
    dup2(stdout_pipe[1], STDERR_FILENO);
    CloseFd(&stdin_pipe[0]);
    CloseFd(&stdin_pipe[1]);
    CloseFd(&stdout_pipe[0]);
    CloseFd(&stdout_pipe[1]);
    execl(binary_path_.c_str(), binary_path_.c_str(), "--stdio", nullptr);
    _exit(127);
  }

  CloseFd(&stdin_pipe[0]);
  CloseFd(&stdout_pipe[1]);

  const std::string command =
      "SCAN\t" + SanitizeProtocolField(request.artifact_id) +
      "\t" + SanitizeProtocolField(request.quarantine_path) +
      "\t" + SanitizeProtocolField(request.source_url) +
      "\t" + SanitizeProtocolField(request.suggested_filename);
  if (!WriteLine(stdin_pipe[1], command, error) ||
      !WriteLine(stdin_pipe[1], "SHUTDOWN", error)) {
    CloseFd(&stdin_pipe[1]);
    CloseFd(&stdout_pipe[0]);
    int status = 0;
    waitpid(child_pid, &status, 0);
    return false;
  }
  CloseFd(&stdin_pipe[1]);

  bool saw_report = false;
  while (true) {
    std::string line;
    if (!ReadLine(stdout_pipe[0], &line, error)) {
      CloseFd(&stdout_pipe[0]);
      int status = 0;
      waitpid(child_pid, &status, 0);
      return false;
    }

    const std::vector<std::string> fields = Split(line, '\t');
    if (fields.empty()) {
      continue;
    }

    if (fields[0] == "REPORT") {
      if (fields.size() < 13) {
        if (error != nullptr) {
          *error = "Artifact scan service returned an incomplete REPORT record.";
        }
        CloseFd(&stdout_pipe[0]);
        int status = 0;
        waitpid(child_pid, &status, 0);
        return false;
      }

      report->artifact_id = fields[1];
      report->quarantine_path = fields[2];
      report->source_url = fields[3];
      report->suggested_filename = fields[4];
      report->detected_name = fields[5];
      try {
        report->size_bytes = static_cast<std::uint64_t>(std::stoull(fields[6]));
        report->risk_score = std::stoi(fields[10]);
      } catch (const std::exception& parse_exception) {
        if (error != nullptr) {
          *error = std::string("Artifact scan service returned unparseable numeric field: ") +
                   parse_exception.what();
        }
        CloseFd(&stdout_pipe[0]);
        int status = 0;
        waitpid(child_pid, &status, 0);
        return false;
      }
      report->mime_guess = fields[7];
      report->sha256 = fields[8];
      report->sha1 = fields[9];
      report->risk_level = fields[11];
      report->heuristics = fields[12].empty() ? std::vector<std::string>()
                                              : Split(fields[12], ',');
      report->summary = fields.size() > 13 ? fields[13] : std::string();
      saw_report = true;
      continue;
    }

    if (fields[0] == "OK") {
      break;
    }

    if (fields[0] == "ERROR") {
      if (error != nullptr) {
        *error = fields.size() > 1 ? fields[1] : "Artifact scan service returned an unspecified error.";
      }
      CloseFd(&stdout_pipe[0]);
      int status = 0;
      waitpid(child_pid, &status, 0);
      return false;
    }
  }

  CloseFd(&stdout_pipe[0]);
  int status = 0;
  waitpid(child_pid, &status, 0);
  if (!saw_report) {
    if (error != nullptr) {
      *error = "Artifact scan service finished without a report.";
    }
    return false;
  }
  return true;
}

}  // namespace veyra
