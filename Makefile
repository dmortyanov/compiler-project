.PHONY: all build clean test demo

all: build

build:
	mkdir -p build && cd build && cmake .. && make compiler

clean:
	rm -rf build demo/showcase.asm demo/showcase.o demo/showcase

test: build
	cd build && ctest --output-on-failure

demo: build
	bash demo/run_demo.sh
