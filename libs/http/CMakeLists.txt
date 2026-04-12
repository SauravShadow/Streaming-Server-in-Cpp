add_library(http STATIC
    src/http_parser.cpp
)

target_include_directories(http PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)