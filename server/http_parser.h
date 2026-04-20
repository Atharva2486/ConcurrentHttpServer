#ifndef SERVER_HTTP_PARSER_H_
#define SERVER_HTTP_PARSER_H_

#include <cstddef>
#include <string>

#include "server/http_types.h"

namespace concurrent_http {

class HttpParser {
 public:
  struct ParseResult {
    bool success = false;
    int status_code = 400;
    std::string error_message;
    HttpRequest request;
  };

  static ParseResult ReadFromSocket(int client_socket,
                                    std::size_t max_header_bytes = 64 * 1024,
                                    std::size_t max_body_bytes = 2 * 1024 *
                                                                 1024);
};

}  // namespace concurrent_http

#endif  // SERVER_HTTP_PARSER_H_
