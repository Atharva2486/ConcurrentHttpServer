#ifndef THREADPOOL_THREAD_POOL_H_
#define THREADPOOL_THREAD_POOL_H_

#include <condition_variable>
#include <cstddef>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace concurrent_http {

// Fixed-size worker pool with a single FIFO queue. The listener thread acts as
// the producer, while workers consume accepted client jobs fairly in arrival
// order.
class ThreadPool {
 public:
  explicit ThreadPool(std::size_t thread_count);
  ~ThreadPool();

  void Start();
  void Stop();
  void Enqueue(std::function<void()> task);

  std::size_t Size() const noexcept;

 private:
  void WorkerLoop();

  mutable std::mutex mutex_;
  std::condition_variable cv_;
  std::queue<std::function<void()>> tasks_;
  std::vector<std::thread> workers_;
  std::size_t thread_count_;
  bool stopping_;
  bool started_;
};

}  // namespace concurrent_http

#endif  // THREADPOOL_THREAD_POOL_H_
