#pragma once

#include "http/http_request.h"

namespace streaming {

class VideoHandler {
public:
  static void handle(int client_fd, const http::HttpRequest &req);
};

} // namespace streaming