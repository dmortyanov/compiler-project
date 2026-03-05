# MiniCompiler - Отчет о статусе Спринта 2

**Спринт:** 2  
**Статус:**  Завершен  
**Дата:** Март 2026

## Цель Спринта 2

Определить грамматику языка и реализовать парсер, строящий Абстрактное синтаксическое дерево (AST) из потока токенов.

---

## Контрольный список соответствия требованиям

### 1. Структура проекта и гигиена репозитория

| ID | Требование | Статус | Примечания |
|----|------------|--------|------------|
| STR-1 | Все требования Sprint 1 выполнены | ✅ Выполнено | Лексер, препроцессор, тесты сохранены |
| STR-2 | Новый код в папке `parser/` | ✅ Выполнено | `src/parser/` с ast.h, parser.h/cpp, ast_printer.h |
| STR-3 | README.md обновлен | ✅ Выполнено | Документация по `parse`, форматам, примерам |

**Обновленная структура проекта:**
```
compiler-project/
├── src/
│   ├── lexer/
│   ├── parser/            # НОВОЕ
│   │   ├── ast.h          # Иерархия узлов AST
│   │   ├── ast_printer.h  # Pretty Printer, DOT, JSON
│   │   ├── parser.h
│   │   └── parser.cpp
│   ├── preprocessor/
│   └── utils/
├── tests/
│   ├── lexer/
│   ├── parser/            # НОВОЕ
│   │   ├── valid/
│   │   │   ├── expressions/
│   │   │   ├── statements/
│   │   │   ├── declarations/
│   │   │   └── full_programs/
│   │   └── invalid/
│   │       └── syntax_errors/
│   └── test_runner/
├── examples/
├── docs/
│   ├── language_spec.md
│   └── grammar.md         # НОВОЕ
└── README.md
```

### 2. Формальная спецификация грамматики

| ID | Требование | Статус | Реализация |
|----|------------|--------|------------|
| GRAM-1 | Документ грамматики в EBNF | ✅ Выполнено | `docs/grammar.md` |
| GRAM-2 | Компоненты: стартовый символ, нетерминалы, терминалы | ✅ Выполнено | Program как стартовый |
| GRAM-3 | Грамматика выражений (9 уровней приоритета) | ✅ Выполнено | Assignment → LogicalOr → ... → Primary |
| GRAM-4 | Грамматика операторов (блоки, условия, циклы) | ✅ Выполнено | Block, If, While, For, Return |
| GRAM-5 | Грамматика объявлений (функции, переменные, структуры) | ✅ Выполнено | FunctionDecl, StructDecl, VarDecl |
| GRAM-6 | Ассоциативность операторов | ✅ Выполнено | Таблица в grammar.md |

**9 уровней приоритета выражений:**

| Уровень | Операторы | Ассоциативность |
|---------|-----------|-----------------|
| 1 | `= += -= *= /=` | Правая |
| 2 | `\|\|` | Левая |
| 3 | `&&` | Левая |
| 4 | `== !=` | Неассоциативные |
| 5 | `< <= > >=` | Неассоциативные |
| 6 | `+ -` | Левая |
| 7 | `* / %` | Левая |
| 8 | `-` (унарный) `!` | Правая (префикс) |
| 9 | Primary, Call, `()` | — |

### 3. Реализация парсера

| ID | Требование | Статус | Реализация |
|----|------------|--------|------------|
| PAR-1 | Интерфейс парсера | ✅ Выполнено | `Parser(tokens)`, `parse()`, ошибки с позициями |
| PAR-2 | Рекурсивный спуск | ✅ Выполнено | Методы для каждого правила грамматики |
| PAR-3 | Обнаружение ошибок | ✅ Выполнено | Ошибки с line/column, ожидаемые токены |
| PAR-4 | Lookahead (1 токен) | ✅ Выполнено | `peek()`, `check()`, `match()`, dangling else |
| PAR-5 | Производительность O(n) | ✅ Выполнено | LL(1), линейный проход |

**Интерфейс парсера:**
```cpp
Parser parser(tokens);
auto ast = parser.parse();           // ProgramNode
const auto& errs = parser.errors();  // ParseError[]
const auto& m = parser.metrics();    // ErrorMetrics
```

### 4. Структура данных AST

| ID | Требование | Статус | Реализация |
|----|------------|--------|------------|
| AST-1 | Иерархия узлов | ✅ Выполнено | ASTNode → ExpressionNode, StatementNode, DeclarationNode |
| AST-2 | Узлы выражений | ✅ Выполнено | Literal, Identifier, Binary, Unary, Call, Assignment |
| AST-3 | Узлы операторов | ✅ Выполнено | Block, ExprStmt, If, While, For, Return, VarDecl |
| AST-4 | Узлы объявлений | ✅ Выполнено | FunctionDecl, StructDecl, ParamNode |
| AST-5 | Паттерн Visitor | ✅ Выполнено | `ASTVisitor` с `visit()` для каждого типа узла |

### 5. Визуализация и вывод AST

| ID | Требование | Статус | Реализация |
|----|------------|--------|------------|
| VIS-1 | Pretty Printer (текст) | ✅ Выполнено | `ASTPrettyPrinter` с отступами и аннотациями |
| VIS-2 | Graphviz DOT | ✅ Выполнено | `ASTDotPrinter` с цветовой кодировкой узлов |
| VIS-3 | JSON | ✅ Выполнено | `ASTJsonPrinter` для машинного чтения |
| VIS-4 | CLI с опциями формата | ✅ Выполнено | `--format text\|dot\|json`, `--verbose` |

**Примеры команд:**
```bash
# Текстовый формат AST
./compiler parse --input examples/factorial.src

# Graphviz DOT
./compiler parse --input examples/factorial.src --format dot --output ast.dot

# JSON
./compiler parse --input examples/factorial.src --format json

# Подробный вывод с метриками
./compiler parse --input examples/factorial.src --verbose
```

**Пример вывода (текст):**
```
Program [line 1]:
  FunctionDecl: factorial -> int [line 1]:
    Parameters: [int n]
    Body [line 1]:
      Block:
        IfStmt [line 2]:
          Condition:
            Binary: <=
              Identifier: n
              Literal: 1
          Then:
            Block:
              Return: 1 [line 2]
        Return: n * factorial(n - 1) [line 3]
```

### 6. Тестирование и верификация

| ID | Требование | Статус | Реализация |
|----|------------|--------|------------|
| TEST-1 | Юнит-тесты для правил грамматики | ✅ Выполнено | 24 valid теста |
| TEST-2 | Структура valid/invalid | ✅ Выполнено | expressions/, statements/, declarations/, full_programs/, syntax_errors/ |
| TEST-3 | Golden Testing | ✅ Выполнено | `.expected` файлы, сравнение через parser_test_runner |
| TEST-4 | Тесты ошибок | ✅ Выполнено | 8 invalid тестов с `.err` |
| TEST-5 | Интеграционные тесты | ✅ Выполнено | Полные программы: factorial, fibonacci, nested_loops |

**Тестовые файлы:**
```
tests/parser/
├── valid/
│   ├── expressions/     (8 тестов)
│   ├── statements/      (8 тестов)
│   ├── declarations/    (5 тестов)
│   └── full_programs/   (3 теста)
└── invalid/
    └── syntax_errors/   (8 тестов)
```

### 7. Восстановление после ошибок (Optional)

| ID | Требование | Статус | Реализация |
|----|------------|--------|------------|
| ERR-1 | Panic mode recovery | ✅ Выполнено | Пропуск до точки синхронизации |
| ERR-2 | Точки синхронизации | ✅ Выполнено | `;`, `}`, ключевые слова (`fn`, `if`, `while`...) |
| ERR-3 | Метрики качества восстановления | ✅ Выполнено | `ErrorMetrics`: total_errors, recovered, tokens_skipped, recovery_rate |
| ERR-4 | Множественные ошибки, лимит | ✅ Выполнено | До 50 ошибок, предотвращение каскада |

**Метрики восстановления (--verbose):**
```
Parse errors: 3
Recovered: 3
Tokens skipped: 7
Recovery rate: 100%
```

---

## Изменения в коде

### Новые файлы

1. `docs/grammar.md` — формальная грамматика EBNF
2. `src/parser/ast.h` — иерархия узлов AST + Visitor
3. `src/parser/ast_printer.h` — Pretty Printer, DOT, JSON
4. `src/parser/parser.h` — интерфейс парсера
5. `src/parser/parser.cpp` — реализация рекурсивного спуска
6. `tests/test_runner/parser_test_runner.cpp` — тестовый раннер парсера
7. `tests/parser/valid/**/*.src` — 24 валидных теста
8. `tests/parser/invalid/**/*.src/.err` — 8 невалидных тестов
9. `examples/factorial.src` — пример программы

### Измененные файлы

1. `CMakeLists.txt` — добавлен parser.cpp, parser_test_runner
2. `src/main.cpp` — команда `parse`, форматы вывода, метрики
3. `README.md` — документация парсера

