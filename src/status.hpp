#pragma once
#include <windows.h>
#include <list>
#include <memory>
#include <mutex>
#include <string>

class Status {
public:
  class Info {
  public:
    Info(HWND hwnd) noexcept : hwnd(hwnd) {
    }

    Info(Info&& other) = delete;
    Info(const Info& other) = delete;
    Info& operator=(Info&& other) = delete;
    Info& operator=(const Info& other) = delete;

    HWND hwnd;
    std::mutex mutex;
    std::list<std::wstring> stack;

    void Update() noexcept {
      for (auto it = stack.rbegin(); it != stack.rend(); ++it) {
        if (const auto pos = it->find_first_not_of(L" \f\n\r\t\v"); pos != std::wstring::npos) {
          SendMessage(hwnd, SB_SETTEXT, 0, reinterpret_cast<LPARAM>(it->data()));
          return;
        }
      }
      SendMessage(hwnd, SB_SETTEXT, 0, reinterpret_cast<LPARAM>(L""));
    }
  };

  class State {
  public:
    State(std::shared_ptr<Info> info, std::wstring text) noexcept : info_(info) {
      std::lock_guard lock(info_->mutex);
      it_ = info_->stack.insert(info_->stack.end(), std::move(text));
      info_->Update();
    }

    State(State&& other) = delete;
    State(const State& other) = delete;
    State& operator=(State&& other) = delete;
    State& operator=(const State& other) = delete;

    ~State() {
      std::lock_guard lock(info_->mutex);
      info_->stack.erase(it_);
      info_->Update();
    }

    void Set(std::wstring text) noexcept {
      std::lock_guard lock(info_->mutex);
      it_->assign(std::move(text));
      info_->Update();
    }

    void Clear() noexcept {
      std::lock_guard lock(info_->mutex);
      it_->clear();
      info_->Update();
    }

  private:
    std::shared_ptr<Info> info_;
    std::list<std::wstring>::iterator it_;
  };

  Status() = default;

  Status(HWND hwnd) noexcept : info_(std::make_shared<Info>(hwnd)) {
    info_->Update();
  }

  State Set(std::wstring text) noexcept {
    return { info_, std::move(text) };
  }

private:
  std::shared_ptr<Info> info_;
};
