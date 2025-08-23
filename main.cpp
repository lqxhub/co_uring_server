#include <iostream>

#include "io_uring.h"

int main() {
  IoUring ioUring(8088);
  if (auto ret = ioUring.Init(); !ret) {
    std::cout << "uring init fail" << ret.error() << std::endl;
    return -1;
  }
  auto t = ioUring.acceptServer();
  std::cout << "Server started on port 8088." << std::endl;
  t.resume();
  ioUring.run();
  return 0;
}
