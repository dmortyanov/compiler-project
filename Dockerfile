FROM ubuntu:22.04

RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    nasm \
    gcc \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY . .

RUN mkdir -p build && cd build && cmake .. && make

ENV PATH="/app/build:${PATH}"

CMD ["/bin/bash"]
