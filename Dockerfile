FROM debian:bookworm-slim AS build

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        build-essential \
        cmake \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src

COPY CMakeLists.txt ./
COPY apps ./apps
COPY libs ./libs

RUN cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
    && cmake --build build --target server --parallel

FROM debian:bookworm-slim

WORKDIR /app

COPY --from=build /src/build/apps/server/server /app/server

RUN mkdir -p /app/video

EXPOSE 9000

CMD ["/app/server"]
