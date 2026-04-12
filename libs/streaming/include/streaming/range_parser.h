#pragma once

#include <string>

namespace streaming {

struct Range {
    size_t start;
    size_t end;
    bool valid;
};

class RangeParser {
public:
    static Range parse(const std::string& range_header, size_t file_size);
};

}