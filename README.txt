What Subaru (me) is building

# High performance video streaming server in C++

Goal: - Accept http request
      - Serve video files
      - Support range request
      - Handle multiple clients concurrently

## Step 1: High level Architecture

Client -> Socket Server -> Connection Handler -> HTTP Parser -> Router -> File Streaming -> engine -> Response writer

## Step 2: Modules required to build

1. Network Layer (Socket Server)

   - Create Server
   - Bind to port
   - Listen for connections
   - Accept clients

2. Concurrency Layer (Thread Pool)

   - Main thread: accept connections
   - Push client socket to thread pool

3. HTTP Parser

   - Method (GET)
   - Path (/video/sample.mp4)
   - Range :(bytes=1000-) -> Headers

4. Router

   If (path starts with "/video/")
   {
      call video handler;
   }

5. Streaming Engine (CORE PART)

- Range Request Handling
   - Open file
   - Seek to byte 1000
   - Send only required chunk

- Respond Headers:
   - HTTP/1.1 206 Partial Content
   - Content-Range: bytes 1000-2000/50000000000
   - Content-Length: 1000
   - Content-Type: video/mp4

6. Response Writer

   - Send headers
   - Send file chunks
   - Handle Partial writes

##  Step -3: Execution Flow

   accept connections -> assign to thread -> read request -> parse http -> route request -> strean file (chunked) -> close connection (or keep alive)

## Step 4: Project Structure

streaming-server/
│
├── CMakeLists.txt                # Root build config
├── README.md
│
├── apps/                         # Entry points (executables)
│   └── server/
│       ├── main.cpp
│       └── CMakeLists.txt
│
├── libs/                         # All reusable modules (STATIC LIBS)
│
│   ├── network/
│   │   ├── include/network/
│   │   │   ├── tcp_server.h
│   │   │   ├── connection.h
│   │   │   └── socket_utils.h
│   │   ├── src/
│   │   │   ├── tcp_server.cpp
│   │   │   ├── connection.cpp
│   │   │   └── socket_utils.cpp
│   │   └── CMakeLists.txt
│
│   ├── http/
│   │   ├── include/http/
│   │   │   ├── http_request.h
│   │   │   ├── http_response.h
│   │   │   ├── http_parser.h
│   │   │   └── http_router.h
│   │   ├── src/
│   │   │   ├── http_parser.cpp
│   │   │   ├── http_router.cpp
│   │   │   └── http_response.cpp
│   │   └── CMakeLists.txt
│
│   ├── streaming/
│   │   ├── include/streaming/
│   │   │   ├── video_handler.h
│   │   │   ├── range_parser.h
│   │   │   └── file_streamer.h
│   │   ├── src/
│   │   │   ├── video_handler.cpp
│   │   │   ├── range_parser.cpp
│   │   │   └── file_streamer.cpp
│   │   └── CMakeLists.txt
│
│   ├── core/                     # Reusable system components
│   │   ├── include/core/
│   │   │   ├── thread_pool.h     # reuse your existing one
│   │   │   ├── task.h
│   │   │   └── config.h
│   │   ├── src/
│   │   │   ├── thread_pool.cpp
│   │   │   └── config.cpp
│   │   └── CMakeLists.txt
│
│   ├── utils/
│   │   ├── include/utils/
│   │   │   ├── logger.h
│   │   │   ├── timer.h
│   │   │   └── file_utils.h
│   │   ├── src/
│   │   │   ├── logger.cpp
│   │   │   ├── timer.cpp
│   │   │   └── file_utils.cpp
│   │   └── CMakeLists.txt
│
├── video/                       # Test media files
│   └── sample.mp4
│
├── configs/                      # Config files
│   └── server_config.yaml
│
├── scripts/                      # Dev scripts
│   ├── build.sh
│   └── run.sh
│
├── tests/                        # Unit / integration tests
│   ├── network_tests.cpp
│   ├── http_tests.cpp
│   └── streaming_tests.cpp
│
└── build/                        # CMake build output (ignored)

## Docker

Build and run with Docker Compose:

```bash
docker compose up --build
```

Then open:

```text
http://localhost:9000/video/
```

The container mounts the local `./video` folder into `/app/video`, so you can add
new media locally without rebuilding the Docker image.

Manual Docker commands:

```bash
docker build -t streaming-server-cpp .
docker run --rm -p 9000:9000 -v "$(pwd)/video:/app/video:ro" streaming-server-cpp
```
