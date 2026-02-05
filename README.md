# MiniCompiler (Sprint 1)

Учебный проект мини-компилятора: в Sprint 1 реализован лексический анализатор для упрощенного C‑подобного языка.

## Команда

- Участники: Олонцев Иван, ИСБ-123

## Сборка (CMake)

```bash
cmake -S . -B build
cmake --build build
```

## Быстрый старт

```bash
./build/compiler lex --input examples/hello.src --output tokens.txt
```

Формат вывода токенов:
```
LINE:COLUMN TOKEN_TYPE "LEXEME" [LITERAL_VALUE]
```

## Тесты

```bash
cd build
ctest --output-on-failure
```

Тест-раннер ожидает:
- Для каждого `tests/lexer/**/name.src` файл ожидаемых токенов `name.tok`.
- Для некорректных тестов — файл ошибок `name.err` (может быть пустым).

## Спецификация языка

См. `docs/language_spec.md`.
