#include "network/tcp_server.h"
#include <arpa/inet.h>
#include <cstring>
#include <iostream>
#include <unistd.h>

namespace Network {
TcpServer::TcpServer(int port, int thread_count) : port_(port), server_fd_(-1) {
  thread_pool_ = std::make_unique<core::ThreadPool>(thread_count);
}

void TcpServer::setupSocket() {
  server_fd_ = socket(AF_INET, SOCK_STREAM, 0);

  int opt = 1;
  setsockopt(server_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

  sockaddr_in address{};
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = INADDR_ANY;
  address.sin_port = htons(port_);

  if (bind(server_fd_, (struct sockaddr *)&address, sizeof(address)) < 0) {
    perror("bind failed");
    exit(EXIT_FAILURE);
  }

  if (listen(server_fd_, 10) < 0) {
    perror("listen failed");
    exit(EXIT_FAILURE);
  }

  std::cout << "Server listening on port " << port_ << std::endl;
}

void TcpServer::setConnectionHandler(ConnectionHandler handler) {
  handler_ = handler;
}

void TcpServer::start() {
  setupSocket();

  while (true) {
    int client_fd = accept(server_fd_, nullptr, nullptr);
    if (client_fd < 0) {
      perror("accept failed");
      continue;
    }

    if (handler_) {
      thread_pool_->enqueue([this, client_fd]() {
        handler_(client_fd);
        close(client_fd);
      });
    }
  }
}

} // namespace Network
