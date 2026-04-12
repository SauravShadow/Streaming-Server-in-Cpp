#include "streaming/video_handler.h"
#include "streaming/file_streamer.h"
#include "streaming/range_parser.h"

#include <cstring>
#include <string>
#include <vector>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

namespace streaming {

namespace {

bool sendAll(int client_fd, const std::string &data) {
  size_t total_sent = 0;

  while (total_sent < data.size()) {
    ssize_t sent =
        send(client_fd, data.data() + total_sent, data.size() - total_sent, 0);
    if (sent <= 0)
      return false;

    total_sent += sent;
  }

  return true;
}

std::string resolveVideoPath(const std::string &request_path) {
  const std::vector<std::string> candidates = {
      "." + request_path, ".." + request_path, "../.." + request_path,
      "../../.." + request_path};

  for (const std::string &candidate : candidates) {
    struct stat st;
    if (stat(candidate.c_str(), &st) == 0)
      return candidate;
  }

  return "." + request_path;
}

} // namespace

void VideoHandler::handle(int client_fd, const http::HttpRequest &req) {

  if (req.method != "GET" && req.method != "HEAD") {
    const std::string method_not_allowed =
        "HTTP/1.1 405 Method Not Allowed\r\n"
        "Allow: GET, HEAD\r\n"
        "Connection: close\r\n"
        "\r\n";
    sendAll(client_fd, method_not_allowed);
    return;
  }

  std::string file_path = resolveVideoPath(req.path);

  struct stat st;
  if (stat(file_path.c_str(), &st) < 0) {
    const std::string not_found =
        "HTTP/1.1 404 Not Found\r\n"
        "Connection: close\r\n"
        "\r\n";
    sendAll(client_fd, not_found);
    return;
  }

  size_t file_size = st.st_size;

  std::string range_header;
  if (req.headers.count("Range")) {
    range_header = req.headers.at("Range");
  }

  Range r = RangeParser::parse(range_header, file_size);
  if (!range_header.empty() && !r.valid) {
    const std::string range_not_satisfiable =
        "HTTP/1.1 416 Range Not Satisfiable\r\n"
        "Content-Range: bytes */" +
        std::to_string(file_size) +
        "\r\n"
        "Connection: close\r\n"
        "\r\n";
    sendAll(client_fd, range_not_satisfiable);
    return;
  }

  size_t start = r.valid ? r.start : 0;
  size_t end = r.valid ? r.end : file_size - 1;

  size_t content_length = end - start + 1;

  std::string response =
      std::string("HTTP/1.1 ") + (r.valid ? "206 Partial Content" : "200 OK") +
      "\r\n"
      "Content-Type: video/mp4\r\n"
      "Content-Length: " +
      std::to_string(content_length) +
      "\r\n"
      "Accept-Ranges: bytes\r\n"
      "Connection: close\r\n";

  if (r.valid) {
    response += "Content-Range: bytes " + std::to_string(start) + "-" +
                std::to_string(end) + "/" + std::to_string(file_size) + "\r\n";
  }

  response += "\r\n";

  if (!sendAll(client_fd, response) || req.method == "HEAD")
    return;

  FileStreamer::stream(client_fd, file_path, start, end);
}

} // namespace streaming
