#ifndef PREFETCH_PREFETCHER_H_
#define PREFETCH_PREFETCHER_H_

#include <condition_variable>
#include <cstddef>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <vector>

#include "cache/cache.h"
#include "logger/logger.h"
#include "metrics/metrics.h"

namespace concurrent_http {

struct PrefetchConfig {
  std::string document_root;
  std::size_t max_prefetch_resources = 4;
  std::size_t max_prefetch_file_bytes = 1024 * 1024;
  double cache_usage_threshold = 0.80;
  std::size_t max_queue_depth = 128;
  std::size_t heavy_load_connection_threshold = 0;
};

class Prefetcher {
 public:
  Prefetcher(PrefetchConfig config, WorkloadAwareCache& cache, Logger& logger,
             MetricsCollector& metrics);
  ~Prefetcher();

  void Start();
  void Stop();
  void ScheduleHtmlPrefetch(const std::string& base_request_path,
                            const std::string& html_body);

 private:
  struct PrefetchJob {
    std::string base_request_path;
    std::string html_body;
  };

  void WorkerLoop();
  void ProcessJob(const PrefetchJob& job);
  std::vector<std::string> ExtractResourceUrls(
      const std::string& html_body) const;

  PrefetchConfig config_;
  WorkloadAwareCache& cache_;
  Logger& logger_;
  MetricsCollector& metrics_;

  std::mutex mutex_;
  std::condition_variable cv_;
  std::queue<PrefetchJob> jobs_;
  std::thread worker_;
  bool stopping_;
  bool started_;
};

}  // namespace concurrent_http

#endif  // PREFETCH_PREFETCHER_H_
