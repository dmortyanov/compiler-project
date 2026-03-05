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

### Опции парсера

| Флаг | Описание |
|------|----------|
| `--input <file>` | Входной файл |
| `--output <file>` | Выходной файл (stdout по умолчанию) |
| `--format text\|dot\|json` | Формат AST (по умолчанию text) |
| `--verbose` | Подробный вывод |

## Тесты

```bash
cd build
ctest -C Debug --output-on-failure
```

Windows: `ctest -C Debug --output-on-failure`

## Спецификация

- Лексика: `docs/language_spec.md`
- Грамматика: `docs/grammar.md`
