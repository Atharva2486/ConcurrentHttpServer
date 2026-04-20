#include "logger/logger.h"

#include <filesystem>
#include <iostream>
#include <sstream>
#include <stdexcept>

#include "utils/file_utils.h"

namespace concurrent_http {

Logger::Logger(const std::string& file_path, bool mirror_to_console)
    : mirror_to_console_(mirror_to_console) {
  namespace fs = std::filesystem;

  const fs::path log_path(file_path);
  if (!log_path.parent_path().empty()) {
    std::error_code ec;
    fs::create_directories(log_path.parent_path(), ec);
  }

  file_.open(file_path, std::ios::app);
  if (!file_.is_open()) {
    throw std::runtime_error("Unable to open log file: " + file_path);
  }
}

Logger::~Logger() {
  std::lock_guard<std::mutex> lock(mutex_);
  if (file_.is_open()) {
    file_.flush();
    file_.close();
  }
}

void Logger::LogRequest(const std::string& client_ip, const std::string& resource,
                        int status_code) {
  std::ostringstream message;
  message << client_ip << " \"" << resource << "\" " << status_code;
  WriteLine("REQUEST", message.str());
}

void Logger::LogInfo(const std::string& message) { WriteLine("INFO", message); }

void Logger::LogError(const std::string& message) {
  WriteLine("ERROR", message);
}

void Logger::WriteLine(const std::string& level, const std::string& message) {
  const std::string line = "[" + utils::CurrentTimestampIso8601() + "] [" +
                           level + "] " + message;

  std::lock_guard<std::mutex> lock(mutex_);
  file_ << line << std::endl;
  if (mirror_to_console_) {
    std::cout << line << std::endl;
  }
}

}  // namespace concurrent_http
