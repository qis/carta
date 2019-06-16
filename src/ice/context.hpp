#pragma once
#include <ice/scheduler.hpp>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>

namespace ice {

class context final : public scheduler<context> {
public:
  void run() noexcept {
    thread_.store(std::this_thread::get_id(), std::memory_order_release);
    std::unique_lock lock{ mutex_ };
    lock.unlock();
    while (true) {
      lock.lock();
      auto head = acquire();
      while (!head) {
        if (stop_.load(std::memory_order_acquire)) {
          lock.unlock();
          return;
        }
        cv_.wait(lock, [&]() {
          head = acquire();
          return head || stop_.load(std::memory_order_acquire);
        });
      }
      lock.unlock();
      while (head) {
        auto next = head->next.load(std::memory_order_relaxed);
        head->resume();
        head = next;
      }
    }
  }

  bool is_current() const noexcept {
    return thread_.load(std::memory_order_acquire) == std::this_thread::get_id();
  }

  void stop(bool stop = true) noexcept {
    stop_.store(stop, std::memory_order_release);
    cv_.notify_all();
  }

  void post(ice::schedule<context>* schedule) noexcept {
    scheduler::post(schedule);
    cv_.notify_one();
  }

private:
  std::atomic_bool stop_ = false;
  std::atomic<std::thread::id> thread_;
  std::condition_variable cv_;
  std::mutex mutex_;
};

}  // namespace ice
