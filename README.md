# MiniCompiler

Учебный проект мини-компилятора для упрощенного C-подобного языка.

## Команда

- Участники: Олонцев Иван, ИСБ-123

## Сборка (CMake)

```bash
cmake -S . -B build
cmake --build build
```

## Быстрый старт

### Лексер

```bash
./build/compiler lex --input examples/hello.src --output tokens.txt
```

### Парсер

```bash
# Текстовый формат AST
./build/compiler parse --input examples/factorial.src

# Graphviz DOT
./build/compiler parse --input examples/factorial.src --format dot --output ast.dot

# JSON
./build/compiler parse --input examples/factorial.src --format json --output ast.json

# С подробным выводом
./build/compiler parse --input examples/factorial.src --verbose
```

### Семантический анализ (Sprint 3)

```bash
# Проверка программы — вывод ошибок
./build/compiler check --input examples/semantic_demo.src

# С подробным отчётом валидации
./build/compiler check --input examples/semantic_demo.src --verbose

# С выводом декорированного AST (типы выражений)
./build/compiler check --input examples/semantic_demo.src --show-types

# Вывод таблицы символов (текст)
./build/compiler symbols --input examples/factorial.src

# Вывод таблицы символов (JSON)
./build/compiler symbols --input examples/factorial.src --format json

# Сохранение результатов в файл
./build/compiler check --input examples/semantic_demo.src --output report.txt
```

### Генерация промежуточного представления (Sprint 4)

```bash
# Получение текстового дампа трехадресного кода
./build/compiler ir --input examples/factorial.src

# Генерация CFG для GraphViz
./build/compiler ir --input examples/factorial.src --format dot --output cfg.dot
dot -Tpng cfg.dot -o cfg.png

# Вывод в формате JSON
./build/compiler ir --input examples/factorial.src --format json --output ir.json

# Вывод с подробной статистикой IR
./build/compiler ir --input examples/factorial.src --stats

# Выполнение peephole оптимизации и вывод отчёта об оптимизациях
./build/compiler ir --input examples/factorial.src --optimize
```

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

| Флаг | Описание |
|------|----------|
| `--input <file>` | Входной файл |
| `--output <file>` | Выходной файл (stdout по умолчанию) |
| `--format text\|dot\|json` | Формат вывода |
| `--verbose` | Подробный вывод |
| `--show-types` | Показать типы в AST (для `check`) |
| `--stats` | Показать статистику (для `ir`) |
| `--optimize` | Включить Peephole оптимизацию (для `ir`) |

## Тесты

```bash
cd build
ctest -C Debug --output-on-failure
```

Windows: `ctest -C Debug --output-on-failure`

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

## Спецификация

- Лексика: `docs/language_spec.md`
- Грамматика: `docs/grammar.md`
