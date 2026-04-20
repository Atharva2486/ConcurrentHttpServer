#include "utils/file_utils.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <system_error>

namespace concurrent_http {
namespace utils {

namespace {

namespace fs = std::filesystem;

std::string StripQueryAndFragment(const std::string& value) {
  const std::size_t separator = value.find_first_of("?#");
  return value.substr(0, separator);
}

bool IsWithinRoot(const fs::path& root, const fs::path& candidate) {
  auto root_it = root.begin();
  auto candidate_it = candidate.begin();
  for (; root_it != root.end() && candidate_it != candidate.end();
       ++root_it, ++candidate_it) {
    if (*root_it != *candidate_it) {
      return false;
    }
  }
  return root_it == root.end();
}

std::tm SafeLocalTime(std::time_t value) {
  std::tm output{};
#if defined(_WIN32)
  localtime_s(&output, &value);
#else
  localtime_r(&value, &output);
#endif
  return output;
}

}  // namespace

bool ReadFileToString(const std::string& path, std::string* out_content) {
  if (out_content == nullptr) {
    return false;
  }

  std::ifstream input(path, std::ios::binary);
  if (!input.is_open()) {
    return false;
  }

  std::ostringstream buffer;
  buffer << input.rdbuf();
  *out_content = buffer.str();
  return input.good() || input.eof();
}

bool GetFileSize(const std::string& path, std::size_t* out_size) {
  if (out_size == nullptr) {
    return false;
  }

  std::error_code ec;
  const auto size = fs::file_size(path, ec);
  if (ec) {
    return false;
  }
  *out_size = static_cast<std::size_t>(size);
  return true;
}

bool FileExists(const std::string& path) {
  std::error_code ec;
  return fs::exists(path, ec);
}

std::string CurrentTimestampIso8601() {
  const auto now = std::chrono::system_clock::now();
  const std::time_t now_time = std::chrono::system_clock::to_time_t(now);
  const std::tm local_time = SafeLocalTime(now_time);

  std::ostringstream stream;
  stream << std::put_time(&local_time, "%Y-%m-%dT%H:%M:%S");
  return stream.str();
}

std::string NormalizeUrlPath(const std::string& request_path) {
  std::string path = StripQueryAndFragment(request_path);
  if (path.empty()) {
    path = "/";
  }

  for (char& ch : path) {
    if (ch == '\\') {
      ch = '/';
    }
  }

  const bool ends_with_slash = !path.empty() && path.back() == '/';
  if (path.front() != '/') {
    path.insert(path.begin(), '/');
  }

  fs::path normalized = fs::path(path).lexically_normal();
  std::string normalized_path = normalized.generic_string();
  if (normalized_path.empty()) {
    normalized_path = "/";
  }
  if (normalized_path.front() != '/') {
    normalized_path.insert(normalized_path.begin(), '/');
  }
  if (ends_with_slash && normalized_path.back() != '/') {
    normalized_path.push_back('/');
  }

  return normalized_path;
}

std::string NormalizeDependentPath(const std::string& base_request_path,
                                  const std::string& resource_url) {
  std::string resource = StripQueryAndFragment(resource_url);
  if (resource.empty()) {
    return "";
  }

  for (char& ch : resource) {
    if (ch == '\\') {
      ch = '/';
    }
  }

  if (resource.rfind("http://", 0) == 0 || resource.rfind("https://", 0) == 0 ||
      resource.rfind("//", 0) == 0 || resource.rfind("data:", 0) == 0) {
    return "";
  }

  std::filesystem::path combined;
  if (!resource.empty() && resource.front() == '/') {
    combined = std::filesystem::path(resource);
  } else {
    const std::filesystem::path base =
        std::filesystem::path(NormalizeUrlPath(base_request_path));
    combined = base.parent_path() / resource;
  }

  std::string normalized_path = combined.lexically_normal().generic_string();
  if (normalized_path.empty()) {
    return "";
  }
  if (normalized_path.front() != '/') {
    normalized_path.insert(normalized_path.begin(), '/');
  }
  return normalized_path;
}

bool ResolveRequestPath(const std::string& document_root,
                        const std::string& request_path,
                        std::string* out_path) {
  if (out_path == nullptr) {
    return false;
  }

  std::error_code ec;
  fs::path root = fs::weakly_canonical(fs::path(document_root), ec);
  if (ec) {
    root = fs::absolute(fs::path(document_root), ec);
    if (ec) {
      return false;
    }
  }

  std::string normalized_path = NormalizeUrlPath(request_path);
  if (normalized_path == "/") {
    normalized_path = "/index.html";
  } else if (!normalized_path.empty() && normalized_path.back() == '/') {
    normalized_path += "index.html";
  }

  fs::path candidate = (root / normalized_path.substr(1)).lexically_normal();
  fs::path canonical_candidate = fs::weakly_canonical(candidate, ec);
  if (ec) {
    canonical_candidate = candidate;
  }

  if (!IsWithinRoot(root, canonical_candidate)) {
    return false;
  }

  *out_path = canonical_candidate.string();
  return true;
}

}  // namespace utils
}  // namespace concurrent_http
