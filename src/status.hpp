#pragma once
#include <windows.h>
#include <list>
#include <mutex>
#include <string>

class Status {
public:
  Status() = default;

  Status(Status&& other) = delete;
  Status(const Status& other) = delete;
  Status& operator=(Status&& other) = delete;
  Status& operator=(const Status& other) = delete;

  class State {
  public:
    State(Status* status, std::wstring text) noexcept : status_(status) {
      std::lock_guard lock(status_->mutex_);
      it_ = status_->stack_.insert(status_->stack_.end(), std::move(text));
      status_->Update();
    }

    State(State&& other) = delete;
    State(const State& other) = delete;
    State& operator=(State&& other) = delete;
    State& operator=(const State& other) = delete;

    ~State() {
      if (status_) {
        std::lock_guard lock(status_->mutex_);
        status_->stack_.erase(it_);
        status_->Update();
      }
    }

    void Set(std::wstring text) noexcept {
      if (status_) {
        std::lock_guard lock(status_->mutex_);
        it_->assign(std::move(text));
        status_->Update();
      }
    }

    void Clear() noexcept {
      if (status_) {
        std::lock_guard lock(status_->mutex_);
        it_->clear();
        status_->Update();
        status_ = nullptr;
      }
    }

    Status* status_{ nullptr };
    std::list<std::wstring>::iterator it_;
  };

  void Create(HWND hwnd) noexcept {
    hwnd_ = hwnd;
    stack_.clear();
    Update();
  }

  State Set(std::wstring text) noexcept {
    return { this, std::move(text) };
  }

private:
  void Update() noexcept {
    for (auto it = stack_.rbegin(); it != stack_.rend(); ++it) {
      if (const auto pos = it->find_first_not_of(L" \f\n\r\t\v"); pos != std::wstring::npos) {
        SendMessage(hwnd_, SB_SETTEXT, 0, reinterpret_cast<LPARAM>(it->data()));
        return;
      }
    }
    SendMessage(hwnd_, SB_SETTEXT, 0, reinterpret_cast<LPARAM>(L""));
  }

  HWND hwnd_{ nullptr };
  std::mutex mutex_;
  std::list<std::wstring> stack_;
};