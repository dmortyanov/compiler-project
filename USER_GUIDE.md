# MiniCompiler — User Guide (Sprint 1–4)
---

## 1) Сборка проекта (Windows / Visual Studio generator)

Запускать из корня `compiler-project`:

```powershell
cmake -S . -B build
cmake --build build --config Debug
```

После сборки исполняемые файлы лежат здесь:
- `build\Debug\compiler.exe`
- `build\Debug\lexer_test_runner.exe`
- `build\Debug\parser_test_runner.exe`
- `build\Debug\integration_test_runner.exe`
- `build\Debug\semantic_test_runner.exe`

---

## 2) Команда `lex` — лексер (токены)

### 2.1 Вывод в консоль

```powershell
.\build\Debug\compiler.exe lex --input examples\hello.src
```
- **stdout**: поток токенов построчно в формате  
  `LINE:COLUMN TOKEN_TYPE "LEXEME" [LITERAL_VALUE]`
- **stderr**: сообщения об ошибках (если есть) в формате  
  `LINE:COLUMN ERROR <message>`

### 2.2 Вывод в файл

```powershell
.\build\Debug\compiler.exe lex --input examples\hello.src --output tokens.txt
```

### 2.3 Что умеет лексер (наглядно)

Лексер распознаёт:
- **Keywords**: `if`, `else`, `while`, `for`, `int`, `float`, `bool`, `return`, `void`, `struct`, `fn`, `true`, `false`
- **Identifiers**
- **Literals**: int/float/string/bool
- **Operators**: `+ - * / % == != < <= > >= && || ! = += -= *= /=`
- **Дополнительно**: `++`, `--`, `->`
- **Delimiters**: `( ) { } [ ] ; , :`
- **Comments**: `//` и `/* ... */` (пропускаются)
- **Line endings**: `\n` и `\r\n`

Быстро проверить `++/--/->`:

```powershell
.\build\Debug\compiler.exe lex --input tests\lexer\valid\test_incdec_arrow.src
```

---

## 3) Команда `parse` — парсер (AST)
Общий вид:

```powershell
.\build\Debug\compiler.exe parse --input <file> [--output <file>] [--format text|dot|json] [--verbose]
```

### 3.1 Текстовый AST (по умолчанию)

```powershell
.\build\Debug\compiler.exe parse --input examples\factorial.src
```

### 3.2 Graphviz DOT

```powershell
.\build\Debug\compiler.exe parse --input examples\factorial.src --format dot --output ast.dot
```

Сбор картинки:

```powershell
dot -Tpng ast.dot -o ast.png
```

### 3.3 JSON

```powershell
.\build\Debug\compiler.exe parse --input examples\factorial.src --format json --output ast.json
```

### 3.4 `--verbose`

```powershell
.\build\Debug\compiler.exe parse --input examples\factorial.src --verbose
```

Что дополнительно печатается в **stderr**:
- количество токенов
- метрики восстановления после ошибок парсера:
  - `Parse errors: ...`
  - `Recovered: ...`
  - `Tokens skipped: ...`
  - `Recovery rate: ...%`
- таблицы областей видимости (symbol tables):
  - строки `SEMANTIC ...` (если найдены проблемы, например неизвестные идентификаторы)
  - `Scopes: N`
  - `scope#id parent=... symbols=...`

---

## 4) Препроцессор

Отдельной команды нет: препроцессор запускается **автоматически** перед `lex` и `parse`.

Поддерживается:
- удаление комментариев `//` и `/* ... */` (с сохранением структуры строк)
- директивы:
  - `#define NAME value`
  - `#undef NAME`
  - `#ifdef NAME` / `#ifndef NAME` / `#endif`
- защита от рекурсивных макросов (выдаётся ошибка)
- комментарии внутри строковых литералов сохраняются

Мини-демо: создайте файл `examples\pp_demo.src`:

```c
#define X 10
fn main() -> int {
    // comment
    int a = X;
#ifdef X
    a++;
#endif
    return a;
}
```

Запуск:

```powershell
.\build\Debug\compiler.exe lex --input examples\pp_demo.src
.\build\Debug\compiler.exe parse --input examples\pp_demo.src --verbose
```

---

## 5) Тесты (CTest) + как увидеть вывод по каждому файлу

### 5.1 Запуск всех тестов (лексер + парсер + интеграция + семантика)

```powershell
cd build
ctest -C Debug -VV
```

`-VV` включает подробный вывод (в т.ч. список `[PASS]/[FAIL]` по каждому `.src`).

### 5.2 Запуск конкретного набора

```powershell
ctest -C Debug -R lexer_tests -VV
ctest -C Debug -R parser_tests -VV
ctest -C Debug -R integration_examples -VV
ctest -C Debug -R semantic_tests -VV
```

Что проверяется:

| Набор                  | Каталоги тестов                                    | Кол-во |
|------------------------|----------------------------------------------------|--------|
| `lexer_tests`          | `tests/lexer/valid`, `tests/lexer/invalid`         | 34     |
| `parser_tests`         | `tests/parser/valid`, `tests/parser/invalid`       | 33     |
| `integration_examples` | `tests/integration/examples`                       | —      |
| `semantic_tests`       | `tests/semantic/valid`, `tests/semantic/invalid`   | 10     |

Семантические тесты:
- **valid/** (3 теста): `type_compatibility`, `nested_scopes`, `complex_programs`
- **invalid/** (7 тестов): `undeclared_variable`, `type_mismatch` ×2, `duplicate_declaration`, `argument_errors` ×2, `scope_errors`

### 5.3 Прямой запуск semantic test runner (без CTest)

```powershell
.\build\Debug\semantic_test_runner.exe tests\semantic\valid tests\semantic\invalid
```

---

## 6) Команда `check` — семантический анализ (Sprint 3)

Полный пайплайн: **preprocessor → lexer → parser → semantic analyzer**.  
Проверяет типы, области видимости, объявления переменных, вызовы функций.

Общий вид:

```powershell
.\build\Debug\compiler.exe check --input <file> [--output <file>] [--verbose] [--show-types]
```

### 6.1 Базовая проверка (вывод ошибок)

```powershell
.\build\Debug\compiler.exe check --input examples\semantic_demo.src
```

Если ошибок нет — пустой отчёт, exit code `0`.  
Если есть ошибки — список в Rust-стиле + exit code `1`.

### 6.2 `--verbose` — отчёт валидации

```powershell
.\build\Debug\compiler.exe check --input examples\semantic_demo.src --verbose
```

Дополнительно печатается:
- количество ошибок / предупреждений
- символы по областям видимости (scopes)
- статистика типов и раскладка памяти (смещения стека)

### 6.3 `--show-types` — декорированное AST

```powershell
.\build\Debug\compiler.exe check --input examples\semantic_demo.src --show-types
```

К выводу добавляется секция `=== Decorated AST ===` — AST, где каждый узел
выражения аннотирован выведенным типом (`resolved_type`).

### 6.4 Всё вместе

```powershell
.\build\Debug\compiler.exe check --input examples\semantic_demo.src --verbose --show-types
```

### 6.5 Сохранение отчёта в файл

```powershell
.\build\Debug\compiler.exe check --input examples\semantic_demo.src --verbose --show-types --output check_report.txt
```

---

## 7) Команда `symbols` — дамп таблицы символов (Sprint 3)

Выводит содержимое таблицы символов после семантического анализа.

Общий вид:

```powershell
.\build\Debug\compiler.exe symbols --input <file> [--format text|json] [--output <file>]
```

### 7.1 Текстовый дамп (по умолчанию)

```powershell
.\build\Debug\compiler.exe symbols --input examples\semantic_demo.src
```

### 7.2 JSON

```powershell
.\build\Debug\compiler.exe symbols --input examples\semantic_demo.src --format json
```

### 7.3 Сохранение в файл

```powershell
.\build\Debug\compiler.exe symbols --input examples\semantic_demo.src --format json --output symbols.json
```

```powershell
.\build\Debug\compiler.exe symbols --input examples\semantic_demo.src --output symbols.txt
```

---

## 8) Демонстрация семантических ошибок

### 8.1 Необъявленная переменная

Файл `tests\semantic\invalid\undeclared_variable\test1.src`:

```c
// Invalid: undeclared variable
fn main() -> int {
    int x = 10;
    int y = x + z;   // z не объявлена!
    return y;
}
```

```powershell
.\build\Debug\compiler.exe check --input tests\semantic\invalid\undeclared_variable\test1.src
```

> Ожидаемый результат: ошибка `undeclared variable 'z'` с указанием позиции.

### 8.2 Несовместимость типов

Файл `tests\semantic\invalid\type_mismatch\test1.src`:

```c
// Invalid: type mismatch in assignment
fn main() -> int {
    int x = true;    // bool → int: type mismatch
    return x;
}
```

```powershell
.\build\Debug\compiler.exe check --input tests\semantic\invalid\type_mismatch\test1.src
```

### 8.3 Корректная программа (с отчётом)

Файл `tests\semantic\valid\complex_programs\test1.src` — факториал + struct:

```powershell
.\build\Debug\compiler.exe check --input tests\semantic\valid\complex_programs\test1.src --verbose --show-types
```

> Ожидаемый результат: 0 ошибок, полный отчёт со всеми типами и scopes.

### 8.4 Проверка всех invalid-тестов одной серией

```powershell
.\build\Debug\compiler.exe check --input tests\semantic\invalid\undeclared_variable\test1.src
.\build\Debug\compiler.exe check --input tests\semantic\invalid\type_mismatch\test1.src
.\build\Debug\compiler.exe check --input tests\semantic\invalid\type_mismatch\test2.src
.\build\Debug\compiler.exe check --input tests\semantic\invalid\duplicate_declaration\test1.src
.\build\Debug\compiler.exe check --input tests\semantic\invalid\argument_errors\test1.src
.\build\Debug\compiler.exe check --input tests\semantic\invalid\argument_errors\test2.src
.\build\Debug\compiler.exe check --input tests\semantic\invalid\scope_errors\test1.src
```

---

## 9) Полный демо-пайплайн (`semantic_demo.src`)

Файл `examples\semantic_demo.src` покрывает все возможности Sprint 3:
structs, несколько функций, вложенные области видимости, вызовы функций,
смешанные типы, циклы `for`/`while`, условия `if`.

Команды для полного прогона всех стадий:

```powershell
# 1. Лексер — поток токенов
.\build\Debug\compiler.exe lex --input examples\semantic_demo.src

# 2. Парсер — AST (текст)
.\build\Debug\compiler.exe parse --input examples\semantic_demo.src

# 3. Парсер — AST (DOT для Graphviz)
.\build\Debug\compiler.exe parse --input examples\semantic_demo.src --format dot --output semantic_ast.dot

# 4. Семантическая проверка (полный отчёт + типы)
.\build\Debug\compiler.exe check --input examples\semantic_demo.src --verbose --show-types

# 5. Таблица символов (текст)
.\build\Debug\compiler.exe symbols --input examples\semantic_demo.src

# 6. Таблица символов (JSON в файл)
.\build\Debug\compiler.exe symbols --input examples\semantic_demo.src --format json --output symbols.json
```

---

## 10) Команда `ir` — Генерация IR (Sprint 4)

Команда `ir` преобразует семантически корректное AST в трехадресный код IR (Intermediate Representation), объединяя инструкции в базовые блоки и CFG (Control Flow Graph).

Общий вид:

```powershell
.\build\Debug\compiler.exe ir --input <file> [--output <file>] [--format text|dot|json] [--stats] [--optimize]
```

### 10.1 Текстовый дамп (по умолчанию)

```powershell
.\build\Debug\compiler.exe ir --input examples\factorial.src
```

### 10.2 Graphviz DOT (CFG визуализация)

Генерирует граф потока управления, раскрашенный цветами (входы — зелёный, возвраты — красный, циклы — синий).

```powershell
.\build\Debug\compiler.exe ir --input examples\factorial.src --format dot --output cfg.dot
dot -Tpng cfg.dot -o cfg.png
```

### 10.3 JSON

Сериализация IR-программы:

```powershell
.\build\Debug\compiler.exe ir --input examples\factorial.src --format json --output ir.json
```

### 10.4 Статистика IR (`--stats`)

Выделяет метрики IR: счетчики базовых блоков, переменных, временных регистров, количество инструкций каждого типа.

```powershell
.\build\Debug\compiler.exe ir --input examples\factorial.src --stats
```

### 10.5 Peephole-оптимизация (`--optimize`)

Включает оптимизатор исходного IR-кода: свёртка констант, алгебраические упрощения, снижение силы операций, удаление недостижимого/мертвого кода, схлопывание цепочек переходов.
Отчёт об оптимизации (какие инструкции были модифицированы или удалены) выводится в `stderr`.

```powershell
.\build\Debug\compiler.exe ir --input examples\factorial.src --optimize
```

---

## 11) Тестирование IR (Sprint 4)

Спринт 4 предоставляет свой тестовый драйвер, проверяющий эталонный выход `ir_to_text()`:

```powershell
.\build\Debug\ir_test_runner.exe tests\ir\generation tests\ir\validation
```

Также все тесты добавлены в CTest, поэтому полный прогон можно выполнить:
```powershell
cd build
ctest -C Debug -VV
```