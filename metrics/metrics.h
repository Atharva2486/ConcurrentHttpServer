#ifndef METRICS_METRICS_H_
#define METRICS_METRICS_H_

#include <atomic>
#include <chrono>
#include <cstddef>
#include <deque>
#include <mutex>

namespace concurrent_http {

struct MetricsSnapshot {
  std::size_t active_connections = 0;
  std::size_t total_requests = 0;
  double requests_per_second = 0.0;
  std::size_t cache_hits = 0;
  std::size_t cache_misses = 0;
  double cache_hit_rate = 0.0;
  std::size_t approximate_memory_usage_bytes = 0;
  std::size_t thread_count = 0;
};

class MetricsCollector {
 public:
  explicit MetricsCollector(std::size_t thread_count);

  void ConnectionOpened();
  void ConnectionClosed();
  void RequestCompleted();
  void RecordCacheHit();
  void RecordCacheMiss();
  void SetApproximateMemoryUsage(std::size_t bytes);

  MetricsSnapshot Snapshot() const;

 private:
  void PruneOldSamplesLocked(
      const std::chrono::steady_clock::time_point& now) const;

  std::atomic<std::size_t> active_connections_;
  std::atomic<std::size_t> total_requests_;
  std::atomic<std::size_t> cache_hits_;
  std::atomic<std::size_t> cache_misses_;
  std::atomic<std::size_t> approximate_memory_usage_bytes_;

  mutable std::mutex rps_mutex_;
  mutable std::deque<std::chrono::steady_clock::time_point> recent_requests_;
  std::size_t thread_count_;
};

}  // namespace concurrent_http

#endif  // METRICS_METRICS_H_
