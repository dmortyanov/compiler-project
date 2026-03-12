# MiniCompiler — User Guide (Sprint 1–2)
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

### 5.1 Запуск всех тестов

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
```

Что проверяется:
- `lexer_tests`: `tests/lexer/valid` и `tests/lexer/invalid`
- `parser_tests`: `tests/parser/valid` и `tests/parser/invalid`
- `integration_examples`: end-to-end на `tests/integration/examples`

---