# MiniCompiler - Отчет о статусе Спринта 1

**Спринт:** 1  
**Статус:**  Завершен  
**Дата:** Февраль 2026

## Цель Спринта 1 / Sprint 1 Goal

Создать фундамент проекта и реализовать лексический анализатор для упрощенного C‑подобного языка.  

---

## Контрольный список соответствия требованиям / Requirements Checklist

### 1. Структура проекта и гигиена репозитория / Project Structure & Hygiene

| ID | Требование | Статус | Примечания |
|----|------------|--------|------------|
| STR-1 | Структура каталогов проекта | ✅ Выполнено | `compiler-project/` с `src/`, `tests/`, `examples/`, `docs/` |
| STR-2 | Система сборки | ✅ Выполнено | CMake (C++17), `CMakeLists.txt` |
| STR-3 | README.md | ✅ Выполнено | Описание, сборка, старт, ссылка на спецификацию |
| STR-4 | Качество кода | ✅ Выполнено | Модульный лексер, без предупреждений |

**Текущая структура проекта:**
```
compiler-project/
├── src/
│   ├── lexer/
│   └── utils/
├── tests/
│   ├── lexer/
│   │   ├── valid/
│   │   └── invalid/
│   └── test_runner/
├── examples/
├── docs/
└── README.md
```

### 2. Спецификация языка / Language Specification

| ID | Требование | Статус | Реализация |
|----|------------|--------|------------|
| LANG-1 | EBNF и лексика | ✅ Выполнено | `docs/language_spec.md` |
| LANG-2 | Ключевые слова | ✅ Выполнено | Все перечислены |
| LANG-3 | Идентификаторы | ✅ Выполнено | Ограничение 255 символов |
| LANG-4 | Литералы | ✅ Выполнено | int/float/string/bool |
| LANG-5 | Операторы/разделители | ✅ Выполнено | Все из требований |
| LANG-6 | Пробелы и комментарии | ✅ Выполнено | `//` и `/* */` |

### 3. Лексер / Lexer

| ID | Требование | Статус | Реализация |
|----|------------|--------|------------|
| LEX-1 | Структура токена | ✅ Выполнено | type/lexeme/line/column/literal |
| LEX-2 | Интерфейс сканера | ✅ Выполнено | `next_token`, `peek_token`, `is_at_end` |
| LEX-3 | Распознавание токенов | ✅ Выполнено | Ключевые, литералы, операторы |
| LEX-4 | Позиции и окончания строк | ✅ Выполнено | `\n` и `\r\n` |
| LEX-5 | Ошибки и восстановление | ✅ Выполнено | Ошибки с позициями |

### 4. Тестирование / Testing

| ID | Требование | Статус | Реализация |
|----|------------|--------|------------|
| TEST-1 | Юнит-тесты токенов | ✅ Выполнено | 20 valid / 10 invalid |
| TEST-2 | Структура тестов | ✅ Выполнено | `tests/lexer/valid|invalid` |
| TEST-3 | Формат вывода | ✅ Выполнено | `LINE:COLUMN TOKEN_TYPE "LEXEME" [LITERAL]` |
| TEST-4 | Авто-раннер | ✅ Выполнено | `tests/test_runner` |

---

## Интерфейс командной строки / CLI

**Команда:**
```
compiler lex --input <file> [--output <file>]
```

---

## Примеры использования / Examples

```bash
# Сборка
cmake -S . -B build
cmake --build build

# Запуск лексера
./build/compiler lex --input examples/hello.src --output tokens.txt

# Запуск тестов
cd build
ctest --output-on-failure
```

Примечания:  
- Флаг пишется как `--output-on-failure`
- В Windows с multi-config генератором используйте `ctest -C Debug --output-on-failure`.

---

## Выходные форматы / Output Formats

**Токены:**
```
LINE:COLUMN TOKEN_TYPE "LEXEME" [LITERAL_VALUE]
```

**Ошибки:**
```
LINE:COLUMN ERROR <message>
```

---

## Изменения в коде / Code Changes

### Новые файлы

1. `CMakeLists.txt`  
2. `README.md`  
3. `docs/language_spec.md`  
4. `src/lexer/token.h`  
5. `src/lexer/token.cpp`  
6. `src/lexer/scanner.h`  
7. `src/lexer/scanner.cpp`  
8. `src/utils/file_utils.h`  
9. `src/utils/file_utils.cpp`  
10. `src/main.cpp`  
11. `tests/test_runner/main.cpp`  
12. `tests/lexer/valid/*.src/.tok`  
13. `tests/lexer/invalid/*.src/.tok/.err`  

---

## Заключение / Conclusion

 **Спринт 1 полностью завершен.**  