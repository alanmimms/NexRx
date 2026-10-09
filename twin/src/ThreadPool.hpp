#pragma once

#include <vector>
#include <thread>
#include <atomic>
#include <functional>
#include <semaphore>
#include <memory>
#include <algorithm>

namespace nexrx {

class ThreadPool {
public:
  explicit ThreadPool(int nThreads = 0) {
    if (nThreads <= 0) {
      nThreads = static_cast<int>(std::thread::hardware_concurrency());
    }
    if (nThreads < 1) {
      nThreads = 1;
    }

    workers.reserve(nThreads);
    for (int i = 0; i < nThreads; ++i) {
      workers.push_back(std::make_unique<WorkerSlot>());
      WorkerSlot* slot = workers.back().get();
      slot->thread = std::thread([slot]() {
        while (true) {
          slot->semStart.acquire();
          if (slot->stopFlag.load(std::memory_order_relaxed)) {
            break;
          }
          if (slot->task) {
            slot->task();
          }
          slot->semDone.release();
        }
      });
    }
  }

  ~ThreadPool() {
    stop();
  }

  ThreadPool(const ThreadPool&) = delete;
  ThreadPool& operator=(const ThreadPool&) = delete;

  int threadCount() const {
    return static_cast<int>(workers.size());
  }

  template <typename F>
  void parallelFor(int count, F&& func) {
    if (count <= 0) {
      return;
    }
    if (count == 1 || workers.empty()) {
      func(0);
      return;
    }

    int nWorkersToUse = std::min(count - 1, static_cast<int>(workers.size()));
    for (int i = 0; i < nWorkersToUse; ++i) {
      int idx = i + 1;
      workers[i]->task = [&func, idx]() {
        func(idx);
      };
      workers[i]->semStart.release();
    }

    func(0);

    for (int i = nWorkersToUse + 1; i < count; ++i) {
      func(i);
    }

    for (int i = 0; i < nWorkersToUse; ++i) {
      workers[i]->semDone.acquire();
    }
  }

  void stop() {
    if (stopped) {
      return;
    }
    stopped = true;
    for (auto& slot : workers) {
      slot->stopFlag.store(true, std::memory_order_release);
      slot->semStart.release();
    }
    for (auto& slot : workers) {
      if (slot->thread.joinable()) {
        slot->thread.join();
      }
    }
    workers.clear();
  }

private:
  struct WorkerSlot {
    std::binary_semaphore semStart{0};
    std::binary_semaphore semDone{0};
    std::atomic<bool> stopFlag{false};
    std::function<void()> task;
    std::thread thread;
  };

  std::vector<std::unique_ptr<WorkerSlot>> workers;
  bool stopped{false};
};

} // namespace nexrx
