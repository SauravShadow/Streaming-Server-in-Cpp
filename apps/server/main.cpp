#include "http/http_parser.h"
#include "network/tcp_server.h"
#include "streaming/video_handler.h"
#include <cstring>
#include <iostream>
#include <signal.h>
#include <sys/socket.h>
#include <unistd.h>

#define BUFFER_SIZE 4096

void handleClient(int client_fd) {
  char buffer[BUFFER_SIZE] = {0};
  int bytes_read = read(client_fd, buffer, sizeof(buffer));

  if (bytes_read <= 0)
    return;

  std::string raw_request(buffer, bytes_read);

  http::HttpRequest req = http::HttpParser::parse(raw_request);

  std::cout << "Method: " << req.method << std::endl;
  std::cout << "Path: " << req.path << std::endl;

  if (req.headers.count("Host")) {
    std::cout << "Host: " << req.headers["Host"] << std::endl;
  }

  // ROUTE FIRST
  // Map root "/" to the video library by default
  if (req.path == "/") {
    req.path = "/video/";
  }

  if (req.path.find("/video/") == 0 || req.path.find("/watch/") == 0) {
    streaming::VideoHandler::handle(client_fd, req);
    return;
  }

  // DEFAULT RESPONSE
  const char *response = "HTTP/1.1 200 OK\r\n"
                         "Content-Type: text/plain\r\n"
                         "Content-Length: 13\r\n"
                         "\r\n"
                         "Hello, World!";

  send(client_fd, response, strlen(response), 0);
}

int main() {
  signal(SIGPIPE, SIG_IGN);

  Network::TcpServer server(9000, 4); // 4 worker threads

  server.setConnectionHandler(handleClient);
  server.start();

  return 0;
}
