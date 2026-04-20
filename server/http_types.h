#ifndef SERVER_HTTP_TYPES_H_
#define SERVER_HTTP_TYPES_H_

#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace concurrent_http {

struct HttpRequest {
  std::string method;
  std::string target;
  std::string path;
  std::string query;
  std::string version;
  std::unordered_map<std::string, std::string> headers;
  std::string body;
};

struct HttpResponse {
  int status_code = 200;
  std::string content_type = "text/plain; charset=utf-8";
  std::string body;
  std::vector<std::pair<std::string, std::string>> headers;
};

}  // namespace concurrent_http

#endif  // SERVER_HTTP_TYPES_H_
