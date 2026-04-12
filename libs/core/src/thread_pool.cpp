#include "core/thread_pool.h"

namespace core {

ThreadPool::ThreadPool(size_t num_threads) : stop_(false) {
  for (size_t i = 0; i < num_threads; ++i) {
    workers_.emplace_back([this]() { workerThread(); });
  }
}

void ThreadPool::workerThread() {
  while (true) {
    std::function<void()> task;

    {
      std::unique_lock<std::mutex> lock(queue_mutex_);

      condition_.wait(lock, [this]() { return stop_ || !tasks_.empty(); });

      if (stop_ && tasks_.empty()) {
        return;
      }

      task = std::move(tasks_.front());
      tasks_.pop();
    }

    task(); // execute outside lock
  }
}

void ThreadPool::enqueue(std::function<void()> task) {
  {
    std::lock_guard<std::mutex> lock(queue_mutex_);

    if (stop_) {
      throw std::runtime_error("enqueue on stopped ThreadPool");
    }

    tasks_.push(std::move(task));
  }

  condition_.notify_one();
}

void ThreadPool::shutdown() {
  {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    stop_ = true;
  }

  condition_.notify_all();

  for (std::thread &worker : workers_) {
    if (worker.joinable()) {
      worker.join();
    }
  }
}

ThreadPool::~ThreadPool() { shutdown(); }

} // namespace core