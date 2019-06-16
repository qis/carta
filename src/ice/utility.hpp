#pragma once
#include <utility>

namespace ice {

template <typename Handler>
class scope_exit {
public:
  explicit scope_exit(Handler handler) noexcept : handler_(std::move(handler)) {
  }

  scope_exit(scope_exit&& other) noexcept : handler_(std::move(other.handler_)), invoke_(other.invoke_) {
    other.invoke_ = false;
  }

  scope_exit(const scope_exit& other) = delete;
  scope_exit& operator=(const scope_exit& other) = delete;

  ~scope_exit() noexcept(noexcept(handler_())) {
    if (invoke_) {
      handler_();
    }
  }

private:
  Handler handler_;
  bool invoke_ = true;
};

template <typename Handler>
inline auto on_scope_exit(Handler&& handler) noexcept {
  return scope_exit<Handler>{ std::forward<Handler>(handler) };
}

}  // namespace ice
