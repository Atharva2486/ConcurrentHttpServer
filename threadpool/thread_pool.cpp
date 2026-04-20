#include "threadpool/thread_pool.h"

#include <stdexcept>
#include <utility>

namespace concurrent_http {

ThreadPool::ThreadPool(std::size_t thread_count)
    : thread_count_(thread_count == 0 ? 1 : thread_count),
      stopping_(false),
      started_(false) {}

ThreadPool::~ThreadPool() { Stop(); }

void ThreadPool::Start() {
  std::lock_guard<std::mutex> lock(mutex_);
  if (started_) {
    return;
  }

  stopping_ = false;
  workers_.reserve(thread_count_);
  for (std::size_t i = 0; i < thread_count_; ++i) {
    workers_.emplace_back(&ThreadPool::WorkerLoop, this);
  }
  started_ = true;
}

void ThreadPool::Stop() {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!started_) {
      return;
    }
    stopping_ = true;
  }
  cv_.notify_all();

  for (std::thread& worker : workers_) {
    if (worker.joinable()) {
      worker.join();
    }
  }

  workers_.clear();
  {
    std::lock_guard<std::mutex> lock(mutex_);
    started_ = false;
    stopping_ = false;
  }
}

void ThreadPool::Enqueue(std::function<void()> task) {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!started_ || stopping_) {
      throw std::runtime_error("Thread pool is not accepting new work");
    }
    tasks_.push(std::move(task));
  }
  cv_.notify_one();
}

std::size_t ThreadPool::Size() const noexcept { return thread_count_; }

void ThreadPool::WorkerLoop() {
  while (true) {
    std::function<void()> task;
    {
      std::unique_lock<std::mutex> lock(mutex_);
      cv_.wait(lock, [this] { return stopping_ || !tasks_.empty(); });
      if (stopping_ && tasks_.empty()) {
        return;
      }
      task = std::move(tasks_.front());
      tasks_.pop();
    }

    task();
  }
}

}  // namespace concurrent_http
