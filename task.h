#ifndef TASK_H
#define TASK_H

#include <coroutine>
#include <exception>
#include <functional>

template <bool initialSuspend> struct Task {
  struct promise_type {
    std::coroutine_handle<> handle;
    std::function<void()> onDone; // 协程结束时调用的清理回调

    auto get_return_object() { return Task{*this}; }

    auto initial_suspend() {
      if constexpr (initialSuspend) {
        return std::suspend_always{};
      } else {
        return std::suspend_never{};
      }
    }

    auto final_suspend() noexcept {
      struct Awaiter {
        bool await_ready() noexcept { return false; }

        void await_suspend(std::coroutine_handle<promise_type> h) noexcept {
          if (h.promise().onDone) {
            h.promise().onDone();
          }
        }

        void await_resume() noexcept {}
      };
      return Awaiter{};
    }

    void unhandled_exception() { std::terminate(); }

    void return_void() {}

    promise_type() = default;
    ~promise_type() = default;
  };

  explicit Task(promise_type &promise)
      : handle_(std::coroutine_handle<promise_type>::from_promise(promise)) {}

  Task(Task &&other) noexcept : handle_(other.handle_) {
    other.handle_ = nullptr;
  }

  Task &operator=(Task &&other) noexcept {
    if (this != &other) {
      if (handle_ && !handle_.done())
        handle_.destroy();
      handle_ = other.handle_;
      other.handle_ = nullptr;
    }
    return *this;
  }

  Task(const Task &) = delete;
  Task &operator=(const Task &) = delete;

  ~Task() {
    if (handle_ && !handle_.done())
      handle_.destroy();
  }

  void resume() { handle_.resume(); }

  void setOnDone(std::function<void()> onDone) {
    handle_.promise().onDone = std::move(onDone);
  }

private:
  std::coroutine_handle<promise_type> handle_;
};

#endif // TASK_H
