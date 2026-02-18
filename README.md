# MiniCompiler

Учебный проект мини-компилятора для упрощённого C-подобного языка.

## Команда

- Олонцев Иван, ИСБ-123

## Требования

- **CMake** >= 3.10
- **Компилятор C++17**: GCC >= 8, Clang >= 7 или MSVC >= 19.14

## Сборка

### Linux (основная платформа)

Установка зависимостей (Ubuntu / Debian):

```bash
sudo apt update
sudo apt install cmake g++ make
```

Сборка:

```bash
cmake -S . -B build
cmake --build build
```

### Windows

С MinGW:

```bash
cmake -S . -B build -G "MinGW Makefiles"
cmake --build build
```

С Visual Studio:

```bash
cmake -S . -B build
cmake --build build --config Release
```

## Использование

### Запуск лексера

```bash
compiler lex --input <файл> [--output <файл>]
```

| Флаг       | Описание                                              |
|------------|-------------------------------------------------------|
| `--input`  | Путь к исходному файлу (обязательный)                 |
| `--output` | Путь для записи токенов; без него вывод идёт в stdout |

### Примеры

Вывод токенов в консоль:

```bash
./build/compiler lex --input examples/hello.src
```

Сохранение в файл:

```bash
./build/compiler lex --input examples/hello.src --output tokens.txt
```

### Пример ввода и вывода

Исходный файл `examples/hello.src`:

```c
fn main() {
    int counter = 42;
    return counter;
}
```

Результат:

```
1:1 KW_FN "fn"
1:4 IDENTIFIER "main"
1:8 LPAREN "("
1:9 RPAREN ")"
1:11 LBRACE "{"
2:5 KW_INT "int"
2:9 IDENTIFIER "counter"
2:17 ASSIGN "="
2:19 INT_LITERAL "42" 42
2:21 SEMICOLON ";"
3:5 KW_RETURN "return"
3:12 IDENTIFIER "counter"
3:19 SEMICOLON ";"
4:1 RBRACE "}"
4:2 END_OF_FILE ""
```

### Формат вывода токенов

```
LINE:COLUMN TOKEN_TYPE "LEXEME" [LITERAL_VALUE]
```

- `LINE` и `COLUMN` — позиция в исходном файле (нумерация с 1).
- `TOKEN_TYPE` — тип токена (см. таблицу ниже).
- `"LEXEME"` — исходный текст токена.
- `LITERAL_VALUE` — значение литерала (только для `INT_LITERAL`, `FLOAT_LITERAL`, `BOOL_LITERAL`, `STRING_LITERAL`).

### Типы токенов

| Категория   | Типы                                                                                      |
|-------------|-------------------------------------------------------------------------------------------|
| Ключевые    | `KW_IF` `KW_ELSE` `KW_WHILE` `KW_FOR` `KW_INT` `KW_FLOAT` `KW_BOOL` `KW_RETURN` `KW_VOID` `KW_STRUCT` `KW_FN` |
| Литералы    | `INT_LITERAL` `FLOAT_LITERAL` `STRING_LITERAL` `BOOL_LITERAL` `IDENTIFIER`               |
| Операторы   | `PLUS` `MINUS` `STAR` `SLASH` `PERCENT` `EQ` `NEQ` `LT` `LTE` `GT` `GTE` `AND` `OR` `NOT` `ASSIGN` `PLUS_ASSIGN` `MINUS_ASSIGN` `STAR_ASSIGN` `SLASH_ASSIGN` |
| Разделители | `LPAREN` `RPAREN` `LBRACE` `RBRACE` `LBRACKET` `RBRACKET` `SEMICOLON` `COMMA` `COLON`   |
| Служебные   | `END_OF_FILE`                                                                             |

### Обработка ошибок

Ошибки лексера выводятся в stderr в формате:

```
LINE:COLUMN ERROR сообщение
```

Лексер продолжает работу после ошибки (error recovery), пропуская невалидный символ.

## Тесты

```bash
cd build
ctest --output-on-failure
```

Тестовые файлы находятся в `tests/lexer/`:

```
tests/lexer/
├── valid/          # корректные исходники
│   ├── test_keywords.src
│   ├── test_keywords.tok    ← ожидаемые токены
│   ├── ...
│
└── invalid/        # исходники с ошибками
    ├── test_invalid_char.src
    ├── test_invalid_char.tok  ← ожидаемые токены
    ├── test_invalid_char.err  ← ожидаемые ошибки
    └── ...
```

Для каждого `.src` файла тест-раннер:
1. Прогоняет препроцессор и лексер.
2. Сравнивает вывод токенов с `.tok` файлом.
3. Сравнивает ошибки с `.err` файлом (если отсутствует — ожидается пустой список ошибок).

## Структура проекта

```
compiler-project/
├── src/
│   ├── lexer/
│   │   ├── token.h / token.cpp       # Token, TokenType, форматирование
│   │   └── scanner.h / scanner.cpp   # Scanner (лексический анализатор)
│   ├── preprocessor/
│   │   └── preprocessor.h / .cpp     # Препроцессор (#define, комментарии)
│   ├── utils/
│   │   └── file_utils.h / .cpp       # Чтение/запись файлов
│   └── main.cpp                      # CLI точка входа
├── tests/
│   ├── lexer/valid/                   # Тесты: корректный ввод
│   ├── lexer/invalid/                 # Тесты: ошибочный ввод
│   └── test_runner/main.cpp          # Автоматический тест-раннер
├── examples/
│   └── hello.src                      # Пример программы
├── docs/
│   └── language_spec.md               # Спецификация языка (EBNF)
├── CMakeLists.txt
└── README.md
```

## Спецификация языка

Полная лексическая грамматика в формате EBNF: [`docs/language_spec.md`](docs/language_spec.md).
