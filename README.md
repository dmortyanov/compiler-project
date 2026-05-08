# MiniCompiler

Учебный проект мини-компилятора для упрощенного C-подобного языка.

## Команда

- Участники: Олонцев Иван, ИСБ-123

## Сборка (CMake)

```bash
# Для Linux / macOS:
cmake -S . -B build
cmake --build build

# Для Windows (Visual Studio):
cmake -S . -B build
cmake --build build --config Debug
```

> **Внимание по путям (Windows vs Linux)**
> Во всех примерах ниже предполагается, что исполняемый файл находится по пути `./build/compiler` (Linux/macOS). 
> **Для Windows (CMD/PowerShell)** используйте путь `.\build\Debug\compiler.exe` (или `Release`) и обратные слеши `\` для путей.
> Например: `.\build\Debug\compiler.exe lex --input examples\hello.src`

## Быстрый старт

### 1. Лексер

```bash
# Linux
./build/compiler lex --input examples/hello.src --output tokens.txt

# Windows
.\build\Debug\compiler.exe lex --input examples\hello.src --output tokens.txt
```

### 2. Парсер

```bash
# Текстовый формат AST (Linux)
./build/compiler parse --input examples/factorial.src

# Текстовый формат AST (Windows)
.\build\Debug\compiler.exe parse --input examples\factorial.src
```

### 3. Семантический анализ (Sprint 3)

```bash
# Проверка программы (Linux)
./build/compiler check --input examples/semantic_demo.src --verbose --show-types

# Проверка программы (Windows)
.\build\Debug\compiler.exe check --input examples\semantic_demo.src --verbose --show-types
```

### 4. Генерация промежуточного представления (Sprint 4)

```bash
# Генерация IR и вывод статистики (Linux)
./build/compiler ir --input examples/factorial.src --stats --optimize

# Генерация IR и вывод статистики (Windows)
.\build\Debug\compiler.exe ir --input examples\factorial.src --stats --optimize
```

### 5. Кодогенерация x86-64 (Sprint 5 и 6)

> **ВАЖНО (Windows)**: Кодогенератор генерирует ассемблер под System V AMD64 ABI (Linux/macOS). Код **не запустится** напрямую под Windows (нужен WSL или Linux). 
> Однако, вы всё равно можете запустить компилятор на Windows, чтобы получить `.asm` файл и просмотреть его:

```cmd
:: Генерация .asm файла на Windows со всеми оптимизациями 6 спринта
.\build\Debug\compiler.exe compile --input examples\factorial.src --regalloc lsra --x86-peephole --optimize
```

**Полный пайплайн (строго Linux или WSL): компиляция → сборка → линковка → запуск**
```bash
# 1. Компиляция исходника нашим компилятором
./build/compiler compile --input examples/factorial.src --output factorial.asm --regalloc lsra --x86-peephole --optimize

# 2. Ассемблирование (nasm)
nasm -f elf64 -o factorial.o factorial.asm
nasm -f elf64 -o runtime.o src/runtime/runtime.asm

# 3. Линковка
ld -o factorial runtime.o factorial.o

# 4. Запуск и проверка кода возврата
./factorial
echo $?   # Ожидается: 120 (5! = 120)
```

#### Архитектура кодогенерации

- **Стратегия**: stack-based codegen — все значения хранятся в стековых слотах `[rbp-N]`
- **ABI**: System V AMD64 — аргументы через `rdi, rsi, rdx, rcx, r8, r9`; возврат в `rax`
- **Scratch-регистры**: `eax` (основной), `ecx` (вспомогательный), `edx` (для div/mod)
- **PHI-узлы**: разрешаются через move-инструкции в конце предшествующего блока
- **Выравнивание**: стек выровнен до 16 байт (ABI-требование)

#### Runtime-библиотека (`src/runtime/runtime.asm`)

| Функция | Описание | Syscall |
|---------|----------|---------|
| `_start` | Точка входа → вызывает `main()` | `exit(60)` |
| `print_int(int)` | Печать числа + `\n` на stdout | `write(1)` |
| `read_int() → int` | Чтение числа из stdin | `read(0)` |
| `exit_program(int)` | Завершение с кодом | `exit(60)` |

### Примеры ошибок семантического анализа

```
semantic error: undeclared identifier: undeclared variable 'z'
  --> program.src:4:18
  | in function 'main'
  = note: did you mean 'x'?

semantic error: type mismatch: cannot initialize 'x' of type 'int' with value of type 'bool'
  --> program.src:3:5
  | in function 'main'
  = expected: int
  = found: bool

semantic error: argument count mismatch: function 'add' expects 2 argument(s), got 1
  --> program.src:7:13
  | in function 'main'
  = expected: 2 arguments
  = found: 1 arguments
  = note: function signature: add(int, int) -> int
```

### Опции

| Команда | Описание |
|---------|----------|
| `lex` | Лексический анализ |
| `parse` | Синтаксический анализ |
| `check` | Семантический анализ |
| `symbols` | Вывод таблицы символов |
| `ir` | Генерация промежуточного представления (IR) |
| `compile` | Компиляция в x86-64 NASM-ассемблер |

| Флаг | Описание |
|------|----------|
| `--input <file>` | Входной файл |
| `--output <file>` | Выходной файл (stdout по умолчанию) |
| `--format text\|dot\|json` | Формат вывода |
| `--verbose` | Подробный вывод |
| `--show-types` | Показать типы в AST (для `check`) |
| `--stats` | Показать статистику (для `ir`) |
| `--optimize` | Включить Peephole оптимизацию IR (для `ir`, `compile`) |
| `--regalloc lsra\|stack` | Стратегия распределения регистров (для `compile`) |
| `--x86-peephole` | Включить x86 оконную оптимизацию (для `compile`) |

## Тесты

Запуск тестов через CTest:

```bash
# Для Linux / macOS:
cd build
ctest -V

# Для Windows:
cd build
ctest -C Debug -V
```

### Семантические тесты

```
tests/semantic/valid/          — программы без ошибок
  type_compatibility/          — совместимость типов
  nested_scopes/               — вложенные области видимости
  complex_programs/            — сложные программы

tests/semantic/invalid/        — программы с ожидаемыми ошибками
  undeclared_variable/         — необъявленные переменные
  type_mismatch/               — несовпадение типов
  duplicate_declaration/       — дублирование объявлений
  argument_errors/             — ошибки аргументов функций
  scope_errors/                — ошибки областей видимости
```

### Тесты IR (Sprint 4)

```
tests/ir/generation/           — тесты генерации (golden-tests)
  expressions/                 — выражения
  control_flow/                — if, loops
  functions/                   — функции
  integration/                 — полные программы
tests/ir/validation/           — валидационные проверки структуры IR
```

### Тесты кодогенерации (Sprint 5)

```
tests/codegen/valid/             — тесты генерации ассемблера
  arithmetic/                    — арифметические операции
  control_flow/                  — if-else, while
  function_calls/                — вызовы функций, рекурсия
  integration/                   — комплексные программы
tests/codegen/scripts/           — скрипт end-to-end тестирования (Linux)
```

#### End-to-end тестирование (Linux)

```bash
# Запуск полного пайплайна: source → asm → nasm → ld → exec
bash tests/codegen/scripts/test_codegen.sh ./build/compiler
```

## Спецификация

- Лексика: `docs/language_spec.md`
- Грамматика: `docs/grammar.md`
