# Архитектура MiniCompiler

MiniCompiler реализует классический многопроходный пайплайн компиляции.

## Пайплайн

```
Source Code (.src)
    │
    ▼
┌─────────────────┐
│  1. Лексер       │  src/lexer/
│  Tokenization    │  Scanner → Token stream
└───────┬─────────┘
        ▼
┌─────────────────┐
│  2. Парсер       │  src/parser/
│  AST Building    │  Recursive descent → AST
└───────┬─────────┘
        ▼
┌─────────────────┐
│  3. Семантика    │  src/semantic/
│  Type Checking   │  Two-pass analysis, scope resolution
└───────┬─────────┘
        ▼
┌─────────────────┐
│  4. IR Generator │  src/ir/
│  Three-Address   │  AST → SSA-style IR (basic blocks, CFG)
└───────┬─────────┘
        ▼
┌─────────────────┐
│  5. Optimizer    │  src/ir/optimizer.cpp, optimization_passes.cpp
│  IR Passes       │  Const folding, CSE, DCE, Copy Prop, Inlining
└───────┬─────────┘
        ▼
┌─────────────────┐
│  6. Codegen      │  src/codegen/
│  x86-64 NASM/GAS │  Stack-based / LSRA, System V ABI
└───────┬─────────┘
        ▼
  Assembly (.asm/.s)
        │
        ▼  (nasm/as → ld/gcc)
  Executable (ELF)
```

## Компоненты

### 1. Лексический анализ (`src/lexer/`)
Преобразует исходный код в поток токенов. Поддерживает ключевые слова, идентификаторы, литералы (int, float, string, bool), операторы и разделители.

### 2. Синтаксический анализ (`src/parser/`)
Строит абстрактное синтаксическое дерево (AST). Использует рекурсивный спуск с восстановлением после ошибок (error recovery).

### 3. Семантический анализ (`src/semantic/`)
Двухпроходный обход AST:
- **Pass 1**: сбор деклараций функций и структур (forward references)
- **Pass 2**: полная проверка типов, scope validation, декорирование AST

Поддерживает: extern-функции, массивы, вложенные scope, подсказки `did you mean?`.

### 4. Генерация промежуточного представления (`src/ir/`)
AST обходится паттерном Visitor. Генерируется линейный трёхадресный код:
- Базовые блоки с CFG (Control Flow Graph)
- PHI-функции (в форме параметров блоков)
- `source_line` в каждой инструкции (для DWARF)

### 5. Оптимизация IR (`src/ir/optimizer.cpp`, `optimization_passes.cpp`)
Конвейер оптимизаций, работающий итеративно до стабилизации:

| Оптимизация | Описание |
|-------------|----------|
| Constant Folding | Вычисление выражений с константами на этапе компиляции |
| Copy Propagation | Замена копий переменных оригиналами |
| CSE | Устранение общих подвыражений |
| DCE | Удаление мёртвого кода |
| Inlining | Встраивание небольших функций (≤10 инструкций) |

### 6. Генерация кода (`src/codegen/`)

- **Стратегии распределения регистров**: стековое или LSRA (Linear Scan Register Allocation)
- **ABI**: System V AMD64 — аргументы через `rdi, rsi, rdx, rcx, r8, r9`; возврат в `rax`
- **Режимы вывода**:
  - NASM (по умолчанию) — для `nasm -f elf64`
  - GAS + DWARF (`--dwarf`) — для `as -g`, с `.file`/`.loc` директивами для отладки

### 7. Runtime (`src/runtime/runtime.asm`)

| Функция | Описание |
|---------|----------|
| `_start` | Точка входа → вызывает `main()` |
| `print_int(int)` | Печать числа + `\n` на stdout |
| `read_int() → int` | Чтение числа из stdin |
| `exit_program(int)` | Завершение с кодом |

## Тестирование

| Тип | Инструмент | Описание |
|-----|-----------|----------|
| **Unit** | Catch2 | Тесты отдельных компонентов (лексер, парсер, IR, codegen) |
| **Integration** | Bash-скрипт | Чёрный ящик: полный пайплайн → проверка exit code |
| **Differential** | Bash-скрипт | Сравнение поведения с GCC |
| **Fuzzing** | Bash-скрипт | Подача случайных данных — crash-test |
| **Property-based** | Bash-скрипт | Оптимизация не меняет наблюдаемое поведение |
| **Mutation** | Bash-скрипт | Мутации в коде → проверка покрытия тестами |

## Профайлинг

- `time` — замер по секундомеру
- `perf stat` — аппаратные счётчики (IPC, cache misses, cycles, branch misses)
- `perf record` + `perf report` — профиль горячих функций

## Сборка и деплой

- **CMake** — основная система сборки
- **Static build** (`-DSTATIC_BUILD=ON`) — для переноса на другой ПК
- **Docker** — изолированная Linux-среда
- **`make package`** — упаковка для USB-флешки (бинарник + демо + runtime)
