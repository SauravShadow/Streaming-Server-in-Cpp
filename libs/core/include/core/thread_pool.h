#pragma once

#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace core {

class ThreadPool {
public:
  explicit ThreadPool(size_t num_threads);
  ~ThreadPool();

  // disable copy
  ThreadPool(const ThreadPool &) = delete;
  ThreadPool &operator=(const ThreadPool &) = delete;

  // submit task
  void enqueue(std::function<void()> task);

  // stop pool gracefully
  void shutdown();

private:
  std::vector<std::thread> workers_;
  std::queue<std::function<void()>> tasks_;

  std::mutex queue_mutex_;
  std::condition_variable condition_;
  std::atomic<bool> stop_;

  void workerThread();
};

} // namespace core