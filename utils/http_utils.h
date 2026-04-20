#ifndef UTILS_HTTP_UTILS_H_
#define UTILS_HTTP_UTILS_H_

#include <string>
#include <utility>
#include <vector>

namespace concurrent_http {
namespace utils {

std::string Trim(const std::string& value);
std::string ToLower(std::string value);
std::string UrlDecode(const std::string& value);
std::string GetMimeType(const std::string& path);
std::string StatusText(int status_code);
std::string HttpDateNow();
std::string EscapeJson(const std::string& value);
std::string BuildHttpResponse(
    int status_code, const std::string& content_type, const std::string& body,
    const std::vector<std::pair<std::string, std::string>>& extra_headers = {});

}  // namespace utils
}  // namespace concurrent_http

#endif  // UTILS_HTTP_UTILS_H_
