#pragma once

#include <string>

namespace streaming {

class FileStreamer {
public:
    static bool stream(int client_fd, const std::string& file_path,
                       size_t start, size_t end);
};

}
