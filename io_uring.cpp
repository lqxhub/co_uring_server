#include <iostream>
#include <netinet/in.h>
#include <sys/socket.h>

#include "io_uring.h"

std::expected<bool, std::string> IoUring::Init() {
  auto result = createListenSocket();
  if (!result) {
    return std::unexpected(result.error()); // Return the error message
  }
  if (io_uring_queue_init(entries_, &ring_, 0) < 0) {
    return std::unexpected(
        "io_uring_queue_init error"); // Initialization failed
  }
  return true;
}

void IoUring::run() {
  while (running_.load()) {
    io_uring_cqe *cqe = nullptr;
    int ret = io_uring_wait_cqe(&ring_, &cqe);
    if (ret < 0) {
      if (ret == -EINTR)
        continue;
      break;
    }
    // user_data 保存着 Op 指针
    auto *op = reinterpret_cast<AwaitableBaseOp *>(cqe->user_data);
    if (!op) {
      io_uring_cqe_seen(&ring_, cqe);
      continue;
    }
    op->SetRes(cqe->res);
    io_uring_cqe_seen(&ring_, cqe);

    op->resume();
  }
}

Task<true> IoUring::acceptServer() {
  while (true) {
    auto clientFd = co_await AwaitableAccept(this, listenFd_);
    std::cout << clientFd << std::endl;
    if (clientFd < 0)
      break;

    set_nonblocking(clientFd);
    auto connId = getConnId();
    auto t = startSession(clientFd, connId);
    t.setOnDone([connId, this]() { sessions_.erase(connId); });
    sessions_.emplace(connId, std::move(t));
  }
  close(listenFd_);
  co_return;
}

Task<false> IoUring::startSession(int fd, uint64_t connId) {
  std::string buffer;
  buffer.resize(1024);
  while (true) {
    auto res = co_await AwaitableRead(this, fd, buffer);
    if (res <= 0) {
      break;
    }
    std::cout << "Received data: ";
    std::cout.write(buffer.data(), res);
    std::cout << std::endl;
    res = co_await AwaitableWrite(this, fd, std::move(std::string(buffer)));
    if (res <= 0) {
      break;
    }
  }
  close(fd);
  co_return;
}

std::expected<bool, std::string> IoUring::createListenSocket() {
  listenFd_ = socket(AF_INET, SOCK_STREAM, 0);
  if (listenFd_ < 0) {
    return std::unexpected("open socket error"); // Socket creation failed
  }
  int yes = 1;
  if (setsockopt(listenFd_, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) < 0) {
    return std::unexpected("setsockopt error"); // Set socket options failed
  }

  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port_);
  addr.sin_addr.s_addr = INADDR_ANY;

  if (bind(listenFd_, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) < 0) {
    return std::unexpected("bind fd fail"); // Binding failed
  }
  if (listen(listenFd_, SOMAXCONN) < 0) {
    return std::unexpected("listen fail"); // Listening failed
  }
  return true;
}
