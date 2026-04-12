add_library(streaming STATIC
    src/range_parser.cpp
    src/file_streamer.cpp
    src/video_handler.cpp
)

target_include_directories(streaming PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(streaming PUBLIC http)