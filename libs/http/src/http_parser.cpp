#include "http/http_parser.h"
#include <sstream>

namespace http {

HttpRequest HttpParser::parse(const std::string &raw_request) {
  HttpRequest req;

  std::istringstream stream(raw_request);
  std::string line;

  // 1. Request Line
  if (std::getline(stream, line)) {
    parseRequestLine(line, req);
  }

  // 2. Headers
  while (std::getline(stream, line) && line != "\r") {
    parseHeaders(line, req);
  }

  return req;
}

void HttpParser::parseRequestLine(const std::string &line, HttpRequest &req) {
  std::istringstream ss(line);
  ss >> req.method >> req.path >> req.version;
}

void HttpParser::parseHeaders(const std::string &line, HttpRequest &req) {
  size_t colon = line.find(":");
  if (colon == std::string::npos)
    return;

  std::string key = line.substr(0, colon);
  std::string value = line.substr(colon + 1);

  // trim spaces
  if (!value.empty() && value[0] == ' ')
    value.erase(0, 1);

  if (!value.empty() && value.back() == '\r')
    value.pop_back();

  req.headers[key] = value;
}

} // namespace http