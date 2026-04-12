#pragma once

#include "http/http_request.h"
#include <string>

namespace http {
class HttpParser {
public:
  static HttpRequest parse(const std::string &raw_request);

private:
  static void parseRequestLine(const std::string &line, HttpRequest &request);
  static void parseHeaders(const std::string &headers_str,
                           HttpRequest &request);
};
} // namespace http