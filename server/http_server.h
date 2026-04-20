#ifndef SERVER_HTTP_SERVER_H_
#define SERVER_HTTP_SERVER_H_

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <thread>

#include "cache/cache.h"
#include "logger/logger.h"
#include "metrics/metrics.h"
#include "prefetch/prefetcher.h"
#include "server/http_types.h"
#include "threadpool/thread_pool.h"

namespace concurrent_http {

struct ServerConfig {
  std::string host = "0.0.0.0";
  std::uint16_t port = 8080;
  std::string document_root = "./www";
  std::string log_file = "./logs/server.log";
  std::size_t thread_count = 4;
  std::size_t max_cache_bytes = 32 * 1024 * 1024;
  double cache_alpha = 1.0;
  double cache_beta = 5.0;
  double prefetch_cache_threshold = 0.80;
  std::size_t max_prefetch_resources = 4;
  std::size_t max_prefetch_file_bytes = 1024 * 1024;
  bool mirror_logs_to_console = true;
};

class HttpServer {
 public:
  explicit HttpServer(ServerConfig config);
  ~HttpServer();

  void Start();
  void Stop();
  bool IsRunning() const noexcept;

 private:
  int CreateListeningSocket();
  void AcceptLoop();
  void HandleClient(int client_fd, const std::string& client_ip);
  HttpResponse HandleRequest(const HttpRequest& request,
                             std::optional<bool>* cache_lookup);
  HttpResponse ServeStaticFile(const HttpRequest& request,
                               std::optional<bool>* cache_lookup);
  HttpResponse BuildMetricsResponse() const;
  HttpResponse BuildMetricsPage() const;
  HttpResponse HandleEchoPost(const HttpRequest& request) const;
  bool SendResponse(int client_fd, const HttpResponse& response) const;
  void UpdateApproximateMemoryMetric();
  std::string BuildMetricsJson() const;

  ServerConfig config_;
  ThreadPool thread_pool_;
  Logger logger_;
  MetricsCollector metrics_;
  WorkloadAwareCache cache_;
  Prefetcher prefetcher_;
  std::thread accept_thread_;
  std::atomic<bool> running_;
  int listen_fd_;
};

}  // namespace concurrent_http

#endif  // SERVER_HTTP_SERVER_H_
