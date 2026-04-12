#include "streaming/range_parser.h"

namespace streaming {

Range RangeParser::parse(const std::string& range_header, size_t file_size) {
    if (file_size == 0) return Range{0, 0, false};

    Range r{0, file_size - 1, false};

    if (range_header.empty()) return r;

    // Example: "bytes=1000-2000"
    size_t eq = range_header.find('=');
    size_t dash = range_header.find('-');

    if (eq == std::string::npos || dash == std::string::npos) return r;

    std::string start_str = range_header.substr(eq + 1, dash - eq - 1);
    std::string end_str = range_header.substr(dash + 1);

    try {
        if (start_str.empty()) {
            size_t suffix_length = std::stoul(end_str);
            if (suffix_length == 0) return r;

            r.start = suffix_length > file_size ? 0 : file_size - suffix_length;
            r.end = file_size - 1;
        } else {
            r.start = std::stoul(start_str);
            r.end = end_str.empty() ? file_size - 1 : std::stoul(end_str);
        }
    } catch (...) {
        return r;
    }

    if (r.start <= r.end && r.end < file_size) {
        r.valid = true;
    }

    return r;
}

}
