#ifndef LOGGER_LOGGER_H_
#define LOGGER_LOGGER_H_

#include <fstream>
#include <mutex>
#include <string>

namespace concurrent_http {

class Logger {
 public:
  explicit Logger(const std::string& file_path, bool mirror_to_console = true);
  ~Logger();

  void LogRequest(const std::string& client_ip, const std::string& resource,
                  int status_code);
  void LogInfo(const std::string& message);
  void LogError(const std::string& message);

 private:
  void WriteLine(const std::string& level, const std::string& message);

  std::mutex mutex_;
  std::ofstream file_;
  bool mirror_to_console_;
};

}  // namespace concurrent_http

#endif  // LOGGER_LOGGER_H_
