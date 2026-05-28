FROM ubuntu:22.04

RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    nasm \
    gcc \
    git \
    linux-tools-generic \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY . .

# Сборка (release, статическая линковка)
RUN mkdir -p build && cd build && \
    cmake .. -DSTATIC_BUILD=ON -DCMAKE_BUILD_TYPE=Release && \
    make compiler

ENV PATH="/app/build:${PATH}"

# Проверка: компилятор запускается
RUN ./build/compiler lex --input demo/showcase.src > /dev/null

CMD ["/bin/bash"]
