# Sprint 3: Semantic Analysis & Symbol Table — Status

## Статус: ✅ Завершён

## Выполненные требования

### 1. Структура проекта (STR)
- [x] **STR-1**: Все требования Sprint 2 соблюдены
- [x] **STR-2**: Новый каталог `src/semantic/` с файлами:
  - `analyzer.h` / `analyzer.cpp` — семантический анализатор
  - `symbol_table.h` / `symbol_table.cpp` — таблица символов
  - `type_system.h` / `type_system.cpp` — система типов
  - `errors.h` / `errors.cpp` — ошибки семантического анализа
- [x] **STR-3**: README обновлён

### 2. Таблица символов (SYM)
- [x] **SYM-1**: Интерфейс — `enter_scope()`, `exit_scope()`, `insert()`, `lookup()`, `lookup_local()`
- [x] **SYM-2**: Поддержка областей видимости — global, function, block, struct, for
- [x] **SYM-3**: Информация о символах — имя, тип, вид, позиция, параметры функций, поля структур
- [x] **SYM-4**: Представление типов — int, float, bool, void, string, struct, function + проверка эквивалентности
- [x] **SYM-5**: Отслеживание смещений стека и размера типов

### 3. Семантический анализатор (SEM)
- [x] **SEM-1**: Интерфейс — `analyze()`, `get_errors()`, `get_symbol_table()`, `dump_decorated_ast()`
- [x] **SEM-2**: Валидация объявлений — дубли, уникальные имена, валидные типы, forward references для функций
- [x] **SEM-3**: Проверка типов — вывод типов выражений, совместимость присваивания, бинарные/унарные операторы
- [x] **SEM-4**: Валидация функций — проверка сигнатур, количества/типов аргументов, типа возврата
- [x] **SEM-5**: Использование переменных — необъявленные переменные, отслеживание инициализации
- [x] **SEM-6**: Управляющий поток — условия if/while/for должны быть bool/numeric

### 4. Сообщения об ошибках (ERR)
- [x] **ERR-1**: Категории ошибок — 12 видов включая undeclared, duplicate, type mismatch и др.
- [x] **ERR-2**: Формат сообщений — тип, описание, позиция, контекст, expected/actual, подсказки
- [x] **ERR-3**: Восстановление после ошибок — error-тип предотвращает каскадные ошибки
- [x] **ERR-4**: Формат вывода в стиле Rust

### 5. Декорированное AST (DEC)
- [x] **DEC-1**: Аннотации типов на каждом узле выражения (`resolved_type`)
- [x] **DEC-2**: Форматы вывода — декорированный AST, дамп таблицы символов, отчёт ошибок
- [x] **DEC-3**: Отчёт валидации — количество ошибок, символы по областям, типы, раскладка памяти

### 6. Тестирование (TEST)
- [x] **TEST-1**: Unit-тесты — операции таблицы символов, совместимость типов, сигнатуры функций
- [x] **TEST-2**: Тестовые программы — valid/ и invalid/ с подкатегориями
- [x] **TEST-3**: Golden testing — сравнение ожидаемого и фактического вывода
- [x] **TEST-4**: Интеграционные тесты — полный пайплайн lex → parse → semantic
- [x] **TEST-5**: Тесты ошибок — точные позиции, корректные сообщения

### 7. Stretch Goal (INF) — Частично
- [x] **INF-1**: Вывод типов из литералов (int, float, bool, string)
- [x] **INF-2**: Вывод типов выражений — бинарные операции, вызовы функций

## Результаты тестирования

```
Lexer tests:  34/34 PASS
Parser tests: 33/33 PASS
Semantic tests: 10/10 PASS
  - Valid: 3/3 (type_compatibility, nested_scopes, complex_programs)
  - Invalid: 7/7 (undeclared_variable, type_mismatch x2, duplicate_declaration,
                   argument_errors x2, scope_errors)
```

## CLI-команды Sprint 3

```bash
compiler check  --input <file> [--verbose] [--show-types] [--output <file>]
compiler symbols --input <file> [--format text|json] [--output <file>]
```
