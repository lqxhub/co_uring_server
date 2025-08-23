#ifndef IOURING_H
#define IOURING_H

#include <atomic>
#include <expected>
#include <liburing.h>
#include <string>
#include <unordered_map>
#include <utility>

#include "task.h"

class IoUring {
public:
  explicit IoUring(int port) : port_(port) {}

  IoUring(const IoUring &) = delete;
  IoUring &operator=(const IoUring &) = delete;
  IoUring(IoUring &&) = delete;
  IoUring &operator=(IoUring &&) = delete;

  ~IoUring() = default;

  void Stop() { running_.store(false); }

  io_uring &Uring() { return ring_; }

  std::expected<bool, std::string> Init();
  void run();
  Task<true> acceptServer();

  Task<false> startSession(int fd, uint64_t connId);

private:
  std::expected<bool, std::string> createListenSocket();

  static int set_nonblocking(int fd) {
    return fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK);
  }

  uint64_t getConnId() { return ++connId; }

  io_uring ring_{};
  uint16_t port_;
  int listenFd_ = -1;
  const int entries_ = 256; // Default number of entries
  std::atomic<bool> running_ = true;

  uint64_t connId = 0; // Connection ID for tracking connections

  std::unordered_map<uint64_t, Task<false>> sessions_;
};

class AwaitableBaseOp {
public:
  explicit AwaitableBaseOp(std::coroutine_handle<> h) : coro_(h) {}

  virtual ~AwaitableBaseOp() = default;

  void resume() {
    if (coro_ && !coro_.done()) {
      coro_.resume();
    }
  }

  void SetRes(int res) { res_ = res; }

  int GetRes() const { return res_; }

protected:
  std::coroutine_handle<> coro_;
  int res_ = 0;
};

class AwaitableAccept {
  IoUring *uring_ = nullptr;
  sockaddr_storage addr_{};
  socklen_t addrlen_{};
  int serverFd_ = 0;

  AwaitableBaseOp *op = nullptr;

public:
  explicit AwaitableAccept(IoUring *uring, int sererFd)
      : uring_(uring), serverFd_(sererFd) {
    addrlen_ = sizeof(addr_);
  }

  AwaitableAccept(AwaitableAccept &&other) noexcept
      : uring_(other.uring_), serverFd_(other.serverFd_) {
    other.uring_ = nullptr;
    other.serverFd_ = -1;
    other.op = nullptr;
  }

  ~AwaitableAccept() = default;

  bool await_ready() const noexcept { return false; }

  void await_suspend(std::coroutine_handle<> h) {
    op = new AwaitableBaseOp(h);

    io_uring_sqe *sqe = io_uring_get_sqe(&uring_->Uring());
    io_uring_prep_accept(sqe, serverFd_, reinterpret_cast<sockaddr *>(&addr_),
                         &addrlen_, 0);
    io_uring_sqe_set_data(sqe, op);
    io_uring_submit(&uring_->Uring());
  }

  int await_resume() const noexcept {
    int res = op->GetRes();
    delete op;
    return res;
  }
};

class AwaitableRead {
  IoUring *uring_ = nullptr;
  int fd_ = 0;
  std::string &buffer_;

  AwaitableBaseOp *op_ = nullptr;

public:
  explicit AwaitableRead(IoUring *ring, const int fd, std::string &buffer)
      : uring_(ring), fd_(fd), buffer_(buffer) {}

  AwaitableRead(const AwaitableRead &) = delete;
  AwaitableRead &operator=(const AwaitableRead &) = delete;

  AwaitableRead(AwaitableRead &&other) noexcept
      : uring_(other.uring_), fd_(other.fd_), buffer_(other.buffer_),
        op_(other.op_) {
    other.op_ = nullptr;
    other.uring_ = nullptr;
  }

  ~AwaitableRead() = default;

  bool await_ready() const noexcept { return false; }

  void await_suspend(std::coroutine_handle<> h) {
    op_ = new AwaitableBaseOp(h);

    io_uring_sqe *sqe = io_uring_get_sqe(&uring_->Uring());
    io_uring_prep_recv(sqe, fd_, buffer_.data(), buffer_.size(), 0);
    io_uring_sqe_set_data(sqe, op_);
    io_uring_submit(&uring_->Uring());
  }

  int await_resume() const noexcept {
    int res = op_->GetRes();
    delete op_;
    return res;
  }
};

class AwaitableWrite {
  IoUring *uring_ = nullptr;
  int fd_ = 0;
  std::string buffer_;
  AwaitableBaseOp *op_ = nullptr;

public:
  AwaitableWrite(IoUring *ring, int fd, std::string &&buffer)
      : uring_(ring), fd_(fd), buffer_(std::move(buffer)) {}

  AwaitableWrite(const AwaitableWrite &) = delete;
  AwaitableWrite &operator=(const AwaitableWrite &) = delete;

  AwaitableWrite(AwaitableWrite &&other) noexcept
      : uring_(other.uring_), fd_(other.fd_), buffer_(std::move(other.buffer_)),
        op_(other.op_) {
    other.op_ = nullptr; // 关键：转移所有权
    other.uring_ = nullptr;
  }

  ~AwaitableWrite() = default;

  bool await_ready() const noexcept { return false; }

  void await_suspend(std::coroutine_handle<> h) {
    op_ = new AwaitableBaseOp(h);

    io_uring_sqe *sqe = io_uring_get_sqe(&uring_->Uring());
    io_uring_prep_write(sqe, fd_, buffer_.data(), buffer_.size(), 0);
    io_uring_sqe_set_data(sqe, op_);
    io_uring_submit(&uring_->Uring());
  }

  int await_resume() const noexcept {
    int res = op_->GetRes();
    delete op_;
    return res;
  }
};

#endif // IOURING_H
