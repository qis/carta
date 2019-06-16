#pragma once
#include <atomic>
#include <experimental/coroutine>
#include <cassert>

namespace ice {

template <typename Scheduler>
class schedule {
public:
  schedule(Scheduler& scheduler, bool post = false) noexcept : scheduler_(scheduler), ready_(!post && scheduler.is_current()) {
  }

#ifdef __INTELLISENSE__
  // clang-format off
  schedule(schedule&& other) noexcept : scheduler_(other.scheduler_), ready_(other.ready_) {}
  schedule(const schedule& other) noexcept : scheduler_(other.scheduler_), ready_(other.ready_) {}
  schedule& operator=(schedule&& other) noexcept { return *this; }
  schedule& operator=(const schedule& other) noexcept { return *this; }
  // clang-format on
#else
  schedule(schedule&& other) = delete;
  schedule(const schedule& other) = delete;
  schedule& operator=(schedule&& other) = delete;
  schedule& operator=(const schedule& other) = delete;
#endif

  ~schedule() = default;

  constexpr bool await_ready() const noexcept {
    return ready_;
  }

  void await_suspend(std::experimental::coroutine_handle<> awaiter) noexcept {
    awaiter_ = awaiter;
    scheduler_.post(this);
  }

  constexpr void await_resume() const noexcept {
  }

  void resume() noexcept {
    awaiter_.resume();
  }

  std::atomic<schedule*> next{ nullptr };

private:
  Scheduler& scheduler_;
  const bool ready_ = true;
  std::experimental::coroutine_handle<> awaiter_;
};

template <typename Context>
class scheduler {
public:
  scheduler() = default;

  scheduler(scheduler&& other) = delete;
  scheduler(const scheduler& other) = delete;
  scheduler& operator=(scheduler&& other) = delete;
  scheduler& operator=(const scheduler& other) = delete;

  ~scheduler() = default;

  ice::schedule<Context> schedule(bool post = false) noexcept {
    return { static_cast<Context&>(*this), post };
  }

protected:
  ice::schedule<Context>* acquire() noexcept {
    auto head = head_.exchange(nullptr, std::memory_order_acquire);
    if (!head || !head->next.load(std::memory_order_relaxed)) {
      return head;
    }
    ice::schedule<Context>* prev = nullptr;
    ice::schedule<Context>* next = nullptr;
    while (head) {
      next = head->next.exchange(prev, std::memory_order_relaxed);
      prev = head;
      head = next;
    }
    return prev;
  }

  void post(ice::schedule<Context>* schedule) noexcept {
    assert(schedule);
    auto head = head_.load(std::memory_order_acquire);
    do {
      schedule->next.store(head, std::memory_order_relaxed);
    } while (!head_.compare_exchange_weak(head, schedule, std::memory_order_release, std::memory_order_acquire));
  }

  void process() {
    auto head = acquire();
    while (head) {
      auto next = head->next.load(std::memory_order_relaxed);
      head->resume();
      head = next;
    }
  }

private:
  std::atomic<ice::schedule<Context>*> head_ = nullptr;
};

}  // namespace ice
