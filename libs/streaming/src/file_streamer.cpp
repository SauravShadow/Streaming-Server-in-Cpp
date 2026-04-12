#include "streaming/file_streamer.h"
#include <cstdio>
#include <cerrno>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <unistd.h>

namespace streaming {

bool FileStreamer::stream(int client_fd, const std::string &file_path,
                          size_t start, size_t end) {

  int fd = open(file_path.c_str(), O_RDONLY);
  if (fd < 0)
    return false;

  off_t offset = start;
  off_t bytes_to_send = end - start + 1;
  off_t expected_bytes = bytes_to_send;

  // macOS sendfile
  int result = sendfile(fd, client_fd, offset, &bytes_to_send, nullptr, 0);

  if (result == -1) {
    if (errno != EPIPE && errno != ENOTCONN && errno != ECONNRESET) {
      perror("sendfile failed");
    }
  }

  close(fd);
  return result == 0 && bytes_to_send == expected_bytes;
}

} // namespace streaming
