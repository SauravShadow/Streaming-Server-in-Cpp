#include "streaming/file_streamer.h"
#include <cerrno>
#include <cstdio>
#include <fcntl.h>

#ifdef _WIN32
#include <io.h>
#include <winsock2.h>
#define read _read
#define open _open
#define close _close
#define lseek _lseek
#define off_t long
#else
#include <sys/socket.h>
#include <unistd.h>
#ifdef __APPLE__
#include <sys/uio.h>
#elif defined(__linux__)
#include <sys/sendfile.h>
#endif
#endif

namespace streaming {

bool FileStreamer::stream(int client_fd, const std::string &file_path,
                          size_t start, size_t end) {

  int open_flags = O_RDONLY;
#ifdef _WIN32
  open_flags |= O_BINARY;
#endif

  int fd = open(file_path.c_str(), open_flags);
  if (fd < 0)
    return false;

  off_t offset = start;
  off_t bytes_remaining = end - start + 1;
  bool success = true;

#if defined(__linux__)
  // 🔥 Optimized (Final): sendfile() -> zero-copy
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
#elif defined(__APPLE__)
  // 🔥 Optimized (Final): sendfile() for macOS
  off_t bytes_to_send = bytes_remaining;
  int result = sendfile(fd, client_fd, offset, &bytes_to_send, nullptr, 0);
  if (result == -1) {
    if (errno != EPIPE && errno != ENOTCONN && errno != ECONNRESET) {
      perror("sendfile failed");
    }
    success = false;
  }
  bytes_remaining -= bytes_to_send;
#else
  // 🔹 Basic (Initial): read() -> send()
  // Works on Windows and provides a robust fallback
  if (lseek(fd, start, SEEK_SET) == -1) {
    close(fd);
    return false;
  }

  char buffer[4096 * 8]; // 32KB buffer for efficiency
  while (bytes_remaining > 0) {
    size_t to_read = (bytes_remaining > (off_t)sizeof(buffer)) ? sizeof(buffer) : (size_t)bytes_remaining;
    int bytes_read = read(fd, buffer, (unsigned int)to_read);
    
    if (bytes_read <= 0) {
      success = false;
      break;
    }

    int bytes_sent = send(client_fd, buffer, bytes_read, 0);
    if (bytes_sent <= 0) {
      success = false;
      break;
    }
    bytes_remaining -= bytes_sent;
  }
#endif

  close(fd);
  return success && bytes_remaining == 0;
}

} // namespace streaming
