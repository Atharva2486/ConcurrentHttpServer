#ifndef UTILS_FILE_UTILS_H_
#define UTILS_FILE_UTILS_H_

#include <cstddef>
#include <string>

namespace concurrent_http {
namespace utils {

bool ReadFileToString(const std::string& path, std::string* out_content);
bool GetFileSize(const std::string& path, std::size_t* out_size);
bool FileExists(const std::string& path);
std::string CurrentTimestampIso8601();
std::string NormalizeUrlPath(const std::string& request_path);
std::string NormalizeDependentPath(const std::string& base_request_path,
                                  const std::string& resource_url);
bool ResolveRequestPath(const std::string& document_root,
                        const std::string& request_path,
                        std::string* out_path);

}  // namespace utils
}  // namespace concurrent_http

#endif  // UTILS_FILE_UTILS_H_
