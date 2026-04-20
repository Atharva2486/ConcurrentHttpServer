#ifndef CACHE_CACHE_H_
#define CACHE_CACHE_H_

#include <array>
#include <chrono>
#include <cstddef>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>

namespace concurrent_http {

struct CacheResult {
  std::shared_ptr<const std::string> content;
  std::string content_type;
  std::size_t file_size = 0;
};

struct CacheStats {
  std::size_t entry_count = 0;
  std::size_t current_memory_usage_bytes = 0;
  std::size_t max_memory_usage_bytes = 0;
  double usage_ratio = 0.0;
};

class WorkloadAwareCache {
 public:
  WorkloadAwareCache(std::size_t max_memory_bytes, double alpha, double beta);

  bool Get(const std::string& key, CacheResult* result);
  bool Put(const std::string& key, const std::string& content,
           const std::string& content_type,
           std::size_t initial_frequency = 1);
  bool Contains(const std::string& key) const;
  CacheStats GetStats() const;
  double GetAlpha() const;
  double GetBeta() const;
  void AdjustWeights(double hit_rate);

 private:
  static constexpr std::size_t kScoreProfileCount = 3;

  enum class ScoreProfile : std::size_t {
    RecencyHeavy = 0,
    Balanced = 1,
    FrequencyHeavy = 2,
  };

  struct CacheEntry {
    std::shared_ptr<const std::string> content;
    std::string content_type;
    std::size_t file_size = 0;
    std::size_t access_frequency = 0;
    std::chrono::steady_clock::time_point last_access;
    std::array<double, kScoreProfileCount> scores{};
    std::array<std::multimap<double, std::string>::iterator,
               kScoreProfileCount>
        eviction_its;
  };

  static ScoreProfile ProfileForWeights(double alpha, double beta);
  static ScoreProfile ProfileForHitRate(double hit_rate);
  static std::pair<double, double> WeightsForProfile(ScoreProfile profile);
  static std::size_t ProfileIndex(ScoreProfile profile);

  double ComputeScoreLocked(
      const CacheEntry& entry,
      ScoreProfile profile,
      const std::chrono::steady_clock::time_point& now) const;
  void RefreshEntryScoresLocked(
      const std::string& key,
      CacheEntry* entry,
      const std::chrono::steady_clock::time_point& now);
  void RemoveFromEvictionIndexesLocked(const CacheEntry& entry);
  void InsertIntoEvictionIndexesLocked(const std::string& key,
                                       CacheEntry* entry);
  void EvictIfNeededLocked(const std::chrono::steady_clock::time_point& now);

  mutable std::mutex mutex_;
  std::unordered_map<std::string, CacheEntry> entries_;
  std::array<std::multimap<double, std::string>, kScoreProfileCount>
      eviction_indexes_;
  std::size_t current_memory_usage_bytes_;
  std::size_t max_memory_usage_bytes_;
  ScoreProfile active_profile_;
};

}  // namespace concurrent_http

#endif  // CACHE_CACHE_H_
