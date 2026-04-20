#include "prefetch/prefetcher.h"

#include <algorithm>
#include <regex>
#include <unordered_set>

#include "utils/file_utils.h"
#include "utils/http_utils.h"

namespace concurrent_http {

Prefetcher::Prefetcher(PrefetchConfig config, WorkloadAwareCache& cache,
                       Logger& logger, MetricsCollector& metrics)
    : config_(std::move(config)),
      cache_(cache),
      logger_(logger),
      metrics_(metrics),
      stopping_(false),
      started_(false) {}

Prefetcher::~Prefetcher() { Stop(); }

void Prefetcher::Start() {
  std::lock_guard<std::mutex> lock(mutex_);
  if (started_) {
    return;
  }

  stopping_ = false;
  started_ = true;
  worker_ = std::thread(&Prefetcher::WorkerLoop, this);
}

void Prefetcher::Stop() {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!started_) {
      return;
    }
    stopping_ = true;
  }
  cv_.notify_all();

  if (worker_.joinable()) {
    worker_.join();
  }

  std::lock_guard<std::mutex> lock(mutex_);
  started_ = false;
  stopping_ = false;
}

void Prefetcher::ScheduleHtmlPrefetch(const std::string& base_request_path,
                                      const std::string& html_body) {
  if (html_body.empty()) {
    return;
  }

  if (cache_.GetStats().usage_ratio >= config_.cache_usage_threshold) {
    return;
  }

  const MetricsSnapshot snapshot = metrics_.Snapshot();
  const std::size_t heavy_load_threshold =
      config_.heavy_load_connection_threshold == 0
          ? std::max<std::size_t>(snapshot.thread_count * 2, 8)
          : config_.heavy_load_connection_threshold;
  if (snapshot.active_connections > heavy_load_threshold) {
    return;
  }

  std::lock_guard<std::mutex> lock(mutex_);
  if (!started_ || stopping_ || jobs_.size() >= config_.max_queue_depth) {
    return;
  }

  jobs_.push(PrefetchJob{base_request_path, html_body});
  cv_.notify_one();
}

void Prefetcher::WorkerLoop() {
  while (true) {
    PrefetchJob job;
    {
      std::unique_lock<std::mutex> lock(mutex_);
      cv_.wait(lock, [this] { return stopping_ || !jobs_.empty(); });
      if (stopping_ && jobs_.empty()) {
        return;
      }

      job = std::move(jobs_.front());
      jobs_.pop();
    }

    try {
      ProcessJob(job);
    } catch (const std::exception& ex) {
      logger_.LogError(std::string("Prefetch worker error: ") + ex.what());
    }
  }
}

void Prefetcher::ProcessJob(const PrefetchJob& job) {
  if (cache_.GetStats().usage_ratio >= config_.cache_usage_threshold) {
    return;
  }

  const MetricsSnapshot snapshot = metrics_.Snapshot();
  const std::size_t heavy_load_threshold =
      config_.heavy_load_connection_threshold == 0
          ? std::max<std::size_t>(snapshot.thread_count * 2, 8)
          : config_.heavy_load_connection_threshold;
  if (snapshot.active_connections > heavy_load_threshold) {
    return;
  }

  std::size_t prefetched = 0;
  for (const std::string& resource_url : ExtractResourceUrls(job.html_body)) {
    if (prefetched >= config_.max_prefetch_resources) {
      break;
    }

    if (cache_.GetStats().usage_ratio >= config_.cache_usage_threshold) {
      break;
    }

    const std::string normalized_path =
        utils::NormalizeDependentPath(job.base_request_path, resource_url);
    if (normalized_path.empty() || cache_.Contains(normalized_path)) {
      continue;
    }

    std::string file_path;
    if (!utils::ResolveRequestPath(config_.document_root, normalized_path,
                                   &file_path)) {
      continue;
    }

    std::size_t file_size = 0;
    if (!utils::GetFileSize(file_path, &file_size) ||
        file_size > config_.max_prefetch_file_bytes) {
      continue;
    }

    std::string content;
    if (!utils::ReadFileToString(file_path, &content)) {
      continue;
    }

    if (!cache_.Put(normalized_path, content, utils::GetMimeType(file_path), 1)) {
      continue;
    }

    metrics_.SetApproximateMemoryUsage(
        cache_.GetStats().current_memory_usage_bytes);
    logger_.LogInfo("Prefetched " + normalized_path);
    ++prefetched;
  }
}

std::vector<std::string> Prefetcher::ExtractResourceUrls(
    const std::string& html_body) const {
  static const std::regex link_pattern(
      R"(<link[^>]*href\s*=\s*["']([^"']+)["'])",
      std::regex_constants::icase);
  static const std::regex script_pattern(
      R"(<script[^>]*src\s*=\s*["']([^"']+)["'])",
      std::regex_constants::icase);
  static const std::regex image_pattern(
      R"(<img[^>]*src\s*=\s*["']([^"']+)["'])",
      std::regex_constants::icase);

  std::vector<std::string> resources;
  std::unordered_set<std::string> seen;

  auto collect_matches = [&](const std::regex& pattern) {
    for (std::sregex_iterator it(html_body.begin(), html_body.end(), pattern),
         end;
         it != end; ++it) {
      const std::string match = (*it)[1].str();
      if (seen.insert(match).second) {
        resources.push_back(match);
      }
      if (resources.size() >= config_.max_prefetch_resources * 2) {
        break;
      }
    }
  };

  collect_matches(link_pattern);
  collect_matches(script_pattern);
  collect_matches(image_pattern);
  return resources;
}

}  // namespace concurrent_http
