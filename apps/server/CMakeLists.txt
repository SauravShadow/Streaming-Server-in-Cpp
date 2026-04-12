add_executable(server main.cpp)

target_link_libraries(server
    core
    network
    http
    streaming
)