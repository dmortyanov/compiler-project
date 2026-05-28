.PHONY: all build clean test demo release package bench profile unit-test integration-test diff-test fuzz-test property-test mutation-test coverage

all: build

build:
	mkdir -p build && cd build && cmake .. && make compiler

clean:
	rm -rf build demo/showcase.asm demo/showcase.o demo/showcase dist

coverage:
	mkdir -p build && cd build && cmake -DENABLE_COVERAGE=ON -DSTATIC_BUILD=OFF -DCMAKE_BUILD_TYPE=Debug .. && make
	bash tests/scripts/coverage.sh

test: build
	cd build && ctest --output-on-failure

# --- Unit тесты (Catch2) ---
unit-test: build
	cd build && cmake .. && make unit_tests && ./unit_tests

# --- Integration (E2E) тесты ---
integration-test: build
	bash tests/scripts/integration_test.sh ./build/compiler

# --- Differential (vs GCC) тесты ---
diff-test: build
	bash tests/scripts/differential_test.sh ./build/compiler

# --- Fuzzing (crash-test) ---
fuzz-test: build
	bash tests/scripts/fuzz_test.sh ./build/compiler 100

# --- Property-based тесты ---
property-test: build
	bash tests/scripts/property_test.sh ./build/compiler

# --- Mutation Testing ---
mutation-test: build
	bash tests/scripts/mutation_test.sh

# --- Все тесты ---
test-all: unit-test integration-test diff-test property-test fuzz-test

# --- Демо ---
demo: build
	bash demo/run_demo.sh

# --- Профайлинг ---
bench: build
	bash tests/scripts/benchmark.sh ./build/compiler time

profile: build
	bash tests/scripts/benchmark.sh ./build/compiler perf

# --- Статическая сборка (для флешки / защиты) ---
release:
	mkdir -p build && cd build && cmake .. -DSTATIC_BUILD=ON -DCMAKE_BUILD_TYPE=Release && make compiler

# --- Упаковка для переноса на другой ПК ---
package: release
	mkdir -p dist
	cp build/compiler dist/
	cp -r demo dist/
	cp src/runtime/runtime.asm dist/
	cp Makefile.deploy dist/Makefile
	@echo ""
	@echo "=== Готово! Папка dist/ готова для копирования на флешку ==="
	@echo "На защитном ПК:"
	@echo "  1. Скопируйте содержимое dist/ в любую папку"
	@echo "  2. Установите nasm: sudo apt install nasm"
	@echo "  3. Запустите: make demo"
