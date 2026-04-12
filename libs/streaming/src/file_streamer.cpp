#include "streaming/file_streamer.h"
#include <cerrno>
#include <cstdio>
#include <fcntl.h>
#include <sys/socket.h>
#ifdef __APPLE__
#include <sys/uio.h>
#elif defined(__linux__)
#include <sys/sendfile.h>
#endif
#include <unistd.h>

namespace streaming {

bool FileStreamer::stream(int client_fd, const std::string &file_path,
                          size_t start, size_t end) {

  int fd = open(file_path.c_str(), O_RDONLY);
  if (fd < 0)
    return false;

  off_t offset = start;
  off_t bytes_remaining = end - start + 1;
  off_t expected_bytes = bytes_remaining;
  bool success = true;

#ifdef __APPLE__
  off_t bytes_to_send = bytes_remaining;
  int result = sendfile(fd, client_fd, offset, &bytes_to_send, nullptr, 0);

  if (result == -1) {
    if (errno != EPIPE && errno != ENOTCONN && errno != ECONNRESET) {
      perror("sendfile failed");
    }
    success = false;
  }

  bytes_remaining -= bytes_to_send;
#elif defined(__linux__)
  while (bytes_remaining > 0) {
    ssize_t sent = sendfile(client_fd, fd, &offset, bytes_remaining);
    if (sent <= 0) {
      if (errno != EPIPE && errno != ENOTCONN && errno != ECONNRESET) {
        perror("sendfile failed");
      }
      success = false;
      break;
    }

    bytes_remaining -= sent;
  }
#else
  success = false;
#endif

  close(fd);
  return success && bytes_remaining == 0 && offset >= static_cast<off_t>(start);
}

} // namespace streaming
