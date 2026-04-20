#include "cache/cache.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <utility>

namespace concurrent_http {

namespace {
constexpr double kEpsilonSeconds = 1.0;

bool NearlyEqual(double left, double right) {
  return std::abs(left - right) < 0.000001;
}
}  // namespace

WorkloadAwareCache::WorkloadAwareCache(std::size_t max_memory_bytes,
                                       double alpha, double beta)
    : current_memory_usage_bytes_(0),
      max_memory_usage_bytes_(std::max<std::size_t>(max_memory_bytes, 1)),
      active_profile_(ProfileForWeights(alpha, beta)) {}

bool WorkloadAwareCache::Get(const std::string& key, CacheResult* result) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto it = entries_.find(key);
  if (it == entries_.end()) {
    return false;
  }

  const auto now = std::chrono::steady_clock::now();
  RemoveFromEvictionIndexesLocked(it->second);
  it->second.access_frequency++;
  it->second.last_access = now;
  RefreshEntryScoresLocked(key, &it->second, now);

  if (result) {
    result->content = it->second.content;
    result->content_type = it->second.content_type;
    result->file_size = it->second.file_size;
  }

  return true;
}

bool WorkloadAwareCache::Put(const std::string& key, const std::string& content,
                             const std::string& content_type,
                             std::size_t initial_frequency) {
  const std::size_t size = content.size();
  if (size > max_memory_usage_bytes_) {
    return false;
  }

  const auto now = std::chrono::steady_clock::now();
  auto shared_content = std::make_shared<const std::string>(content);

  std::lock_guard<std::mutex> lock(mutex_);

  auto it = entries_.find(key);
  if (it != entries_.end()) {
    current_memory_usage_bytes_ -= it->second.file_size;
    RemoveFromEvictionIndexesLocked(it->second);
    entries_.erase(it);
  }

  CacheEntry entry;
  entry.content = shared_content;
  entry.content_type = content_type;
  entry.file_size = size;
  entry.access_frequency = std::max<std::size_t>(1, initial_frequency);
  entry.last_access = now;
  for (std::size_t i = 0; i < kScoreProfileCount; ++i) {
    entry.scores[i] =
        ComputeScoreLocked(entry, static_cast<ScoreProfile>(i), now);
  }

  auto inserted = entries_.emplace(key, entry);
  InsertIntoEvictionIndexesLocked(key, &inserted.first->second);

  current_memory_usage_bytes_ += size;

  EvictIfNeededLocked(now);
  return true;
}

bool WorkloadAwareCache::Contains(const std::string& key) const {
  std::lock_guard<std::mutex> lock(mutex_);
  return entries_.count(key) != 0;
}

CacheStats WorkloadAwareCache::GetStats() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return {
      entries_.size(),
      current_memory_usage_bytes_,
      max_memory_usage_bytes_,
      static_cast<double>(current_memory_usage_bytes_) / max_memory_usage_bytes_
  };
}

WorkloadAwareCache::ScoreProfile WorkloadAwareCache::ProfileForWeights(
    double alpha, double beta) {
  if (NearlyEqual(alpha, 0.5) && NearlyEqual(beta, 8.0)) {
    return ScoreProfile::RecencyHeavy;
  }
  if (NearlyEqual(alpha, 2.0) && NearlyEqual(beta, 3.0)) {
    return ScoreProfile::FrequencyHeavy;
  }
  return ScoreProfile::Balanced;
}

WorkloadAwareCache::ScoreProfile WorkloadAwareCache::ProfileForHitRate(
    double hit_rate) {
  if (hit_rate < 0.5) {
    return ScoreProfile::RecencyHeavy;
  }
  if (hit_rate > 0.8) {
    return ScoreProfile::FrequencyHeavy;
  }
  return ScoreProfile::Balanced;
}

std::pair<double, double> WorkloadAwareCache::WeightsForProfile(
    ScoreProfile profile) {
  switch (profile) {
    case ScoreProfile::RecencyHeavy:
      return {0.5, 8.0};
    case ScoreProfile::FrequencyHeavy:
      return {2.0, 3.0};
    case ScoreProfile::Balanced:
    default:
      return {1.0, 5.0};
  }
}

std::size_t WorkloadAwareCache::ProfileIndex(ScoreProfile profile) {
  return static_cast<std::size_t>(profile);
}

double WorkloadAwareCache::ComputeScoreLocked(
    const CacheEntry& entry,
    ScoreProfile profile,
    const std::chrono::steady_clock::time_point& now) const {
  const double age =
      std::chrono::duration<double>(now - entry.last_access).count();
  const double recency = 1.0 / (age + kEpsilonSeconds);
  const auto weights = WeightsForProfile(profile);

  return weights.first * entry.access_frequency + weights.second * recency;
}

void WorkloadAwareCache::RefreshEntryScoresLocked(
    const std::string& key,
    CacheEntry* entry,
    const std::chrono::steady_clock::time_point& now) {
  for (std::size_t i = 0; i < kScoreProfileCount; ++i) {
    const ScoreProfile profile = static_cast<ScoreProfile>(i);
    entry->scores[i] = ComputeScoreLocked(*entry, profile, now);
    entry->eviction_its[i] = eviction_indexes_[i].emplace(entry->scores[i], key);
  }
}

void WorkloadAwareCache::RemoveFromEvictionIndexesLocked(
    const CacheEntry& entry) {
  for (std::size_t i = 0; i < kScoreProfileCount; ++i) {
    eviction_indexes_[i].erase(entry.eviction_its[i]);
  }
}

void WorkloadAwareCache::InsertIntoEvictionIndexesLocked(
    const std::string& key,
    CacheEntry* entry) {
  for (std::size_t i = 0; i < kScoreProfileCount; ++i) {
    entry->eviction_its[i] = eviction_indexes_[i].emplace(entry->scores[i], key);
  }
}

void WorkloadAwareCache::EvictIfNeededLocked(
    const std::chrono::steady_clock::time_point& /*now*/) {
  if (current_memory_usage_bytes_ <= max_memory_usage_bytes_) {
    return;
  }

  const std::size_t active_index = ProfileIndex(active_profile_);
  while (current_memory_usage_bytes_ > max_memory_usage_bytes_ &&
         !eviction_indexes_[active_index].empty()) {
    const auto victim = eviction_indexes_[active_index].begin();
    const std::string key = victim->second;

    auto it = entries_.find(key);
    if (it == entries_.end()) {
      eviction_indexes_[active_index].erase(victim);
      continue;
    }

    RemoveFromEvictionIndexesLocked(it->second);
    current_memory_usage_bytes_ -= it->second.file_size;
    entries_.erase(it);
  }
}

double WorkloadAwareCache::GetAlpha() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return WeightsForProfile(active_profile_).first;
}

double WorkloadAwareCache::GetBeta() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return WeightsForProfile(active_profile_).second;
}

void WorkloadAwareCache::AdjustWeights(double hit_rate) {
  std::lock_guard<std::mutex> lock(mutex_);
  active_profile_ = ProfileForHitRate(hit_rate);
}

}  // namespace concurrent_http
