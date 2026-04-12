#pragma once

#include "core/thread_pool.h"
#include <functional>
#include <memory>

namespace Network {
class TcpServer {
public:
  using ConnectionHandler = std::function<void(int client_fd)>;

  TcpServer(int port, int thread_count);

  void start(); // start listening loop
  void setConnectionHandler(ConnectionHandler handler);

private:
  int port_;
  int server_fd_;

  ConnectionHandler handler_;
  std::unique_ptr<core::ThreadPool> thread_pool_;
  void setupSocket();
};
} // namespace Network