#!/usr/bin/env bash
# tests/scripts/coverage.sh

# Colors for output
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
RED='\033[0;31m'
BLUE='\033[0;34m'
BOLD='\033[1m'
NC='\033[0m' # No Color

echo -e "${BOLD}=== Сбор статистики покрытия кода (Code Coverage) ===${NC}\n"

# Очистка предыдущих файлов покрытия перед запуском
find build -name "*.gcda" -delete 2>/dev/null

# Запуск тестов
echo -e "${BLUE}Запуск unit-тестов и тестовых сценариев...${NC}"
cd build || exit 1
ctest --output-on-failure
cd ..

# Запуск E2E интеграционных, дифференциальных и property-based тестов для полного покрытия
echo -e "\n${BLUE}Запуск интеграционных, дифференциальных и property-based тестов...${NC}"
bash tests/scripts/integration_test.sh ./build/compiler >/dev/null 2>&1
bash tests/scripts/differential_test.sh ./build/compiler >/dev/null 2>&1
bash tests/scripts/property_test.sh ./build/compiler >/dev/null 2>&1

echo -e "\n${BLUE}Запуск дополнительных CLI-команд и флагов для максимального покрытия (Peephole, LSRA, DWARF, AST/IR formats)...${NC}"
# 1. Сборка с разными опциями кодогенерации (LSRA, Peephole, DWARF)
./build/compiler compile --input tests/codegen/valid/arithmetic/simple_add.src --output /dev/null --optimize --inline --x86-peephole --regalloc lsra --dwarf >/dev/null 2>&1
./build/compiler compile --input tests/codegen/valid/control_flow/if_else.src --output /dev/null --optimize --inline --x86-peephole --regalloc lsra --dwarf >/dev/null 2>&1

# 2. Вызов лексера
./build/compiler lex --input demo/showcase.src --output /dev/null >/dev/null 2>&1

# 3. Вызов парсера с разными форматами
./build/compiler parse --input demo/showcase.src --output /dev/null --format text --verbose >/dev/null 2>&1
./build/compiler parse --input demo/showcase.src --output /dev/null --format json >/dev/null 2>&1
./build/compiler parse --input demo/showcase.src --output /dev/null --format dot >/dev/null 2>&1

# 4. Вызов семантического анализатора с типами
./build/compiler check --input demo/showcase.src --output /dev/null --verbose --show-types >/dev/null 2>&1

# 5. Вызов дампа таблицы символов
./build/compiler symbols --input demo/showcase.src --output /dev/null --format text >/dev/null 2>&1
./build/compiler symbols --input demo/showcase.src --output /dev/null --format json >/dev/null 2>&1

# 6. Вызов генерации IR с разными форматами
./build/compiler ir --input demo/showcase.src --output /dev/null --format text --stats --optimize --inline >/dev/null 2>&1
./build/compiler ir --input demo/showcase.src --output /dev/null --format json --stats --optimize --inline >/dev/null 2>&1
./build/compiler ir --input demo/showcase.src --output /dev/null --format dot --stats --optimize --inline >/dev/null 2>&1

# 7. Вызов компилятора на некорректном файле для покрытия веток с ошибками в CLI
./build/compiler compile --input non_existent_file.src --output /dev/null >/dev/null 2>&1
./build/compiler symbols --input non_existent_file.src --format json >/dev/null 2>&1

# 8. Запуск на всех семантически некорректных файлах для покрытия веток ошибок анализатора
for bad_src in $(find tests/semantic/invalid -name "*.src" 2>/dev/null); do
    ./build/compiler parse --input "$bad_src" --output /dev/null --format text --verbose >/dev/null 2>&1
    ./build/compiler check --input "$bad_src" --output /dev/null >/dev/null 2>&1
    ./build/compiler symbols --input "$bad_src" --format json >/dev/null 2>&1
done

# 9. Запуск на всех синтаксически некорректных файлах для покрытия веток error recovery парсера
for bad_src in $(find tests/parser/invalid -name "*.src" 2>/dev/null); do
    ./build/compiler parse --input "$bad_src" --output /dev/null --format text --verbose >/dev/null 2>&1
done

echo -e "\n${BOLD}=== Результаты покрытия кода по файлам ===${NC}\n"
printf "%-45s | %-10s | %-20s\n" "Файл" "Покрытие" "Строки (Покр/Всего)"
echo "----------------------------------------------+------------+---------------------"

TOTAL_LINES=0
TOTAL_COVERED=0

# Находим все cpp файлы под src (исключая файлы, которые не входят в библиотеку или основной компилятор, если есть)
FILES=$(find src -name "*.cpp" | sort)

for f in $FILES; do
    # Ищем соответствующий .gcda файл в каталоге сборки
    gcda=$(find build/CMakeFiles -path "*/$f.gcda" 2>/dev/null | head -n 1)
    
    if [ -n "$gcda" ]; then
        # Вызываем gcov и извлекаем данные с помощью awk
        gcov_out=$(gcov -n -o "$gcda" "$f" 2>/dev/null)
        
        stats=$(echo "$gcov_out" | awk -v target="$f" '
            BEGIN {
                percent = 0.0;
                total = 0;
                covered = 0;
                color = "'"$RED"'";
                green = "'"$GREEN"'";
                yellow = "'"$YELLOW"'";
                found = 0;
                matched = 0;
            }
            /^File / {
                if (index($0, target) > 0) {
                    matched = 1;
                } else {
                    matched = 0;
                }
                next;
            }
            matched && /Lines executed/ {
                # Lines executed:93.45% of 106
                split($0, a, ":");
                split(a[2], b, "%");
                split(b[2], c, " ");
                percent = b[1] + 0.0;
                total = c[2] + 0;
                covered = int(percent * total / 100 + 0.5);
                color = "'"$RED"'";
                if (percent >= 80.0) color = green;
                else if (percent >= 50.0) color = yellow;
                found = 1;
                matched = 0;
                exit;
            }
            matched && /No executable lines/ {
                percent = 100.00;
                total = 0;
                covered = 0;
                color = green;
                found = 1;
                matched = 0;
                exit;
            }
            END {
                if (found) {
                    print percent, covered, total, color;
                }
            }
        ')
        
        if [ -n "$stats" ]; then
            read -r percent covered total color <<< "$stats"
            
            if [ "$total" -gt 0 ]; then
                TOTAL_LINES=$((TOTAL_LINES + total))
                TOTAL_COVERED=$((TOTAL_COVERED + covered))
                printf "%-45s | %b%6.2f%%%b     | %d/%d\n" "$f" "$color" "$percent" "$NC" "$covered" "$total"
            else
                printf "%-45s | %b%6.2f%%%b     | 0/0 (нет исполняемых строк)\n" "$f" "$GREEN" "100.00" "$NC"
            fi
        else
            printf "%-45s | %b%6.2f%%%b     | 0/0 (ошибка gcov)\n" "$f" "$RED" "0.00" "$NC"
        fi
    else
        printf "%-45s | %b%6.2f%%%b     | 0/0 (не скомпилирован)\n" "$f" "$RED" "0.00" "$NC"
    fi
done

echo "----------------------------------------------+------------+---------------------"

if [ "$TOTAL_LINES" -gt 0 ]; then
    OVERALL_PERCENT=$(echo "scale=4; $TOTAL_COVERED * 100 / $TOTAL_LINES" | bc -l)
    
    # Цвет для общего покрытия
    if (( $(echo "$OVERALL_PERCENT >= 80.0" | bc -l) )); then
        COLOR=$GREEN
    elif (( $(echo "$OVERALL_PERCENT >= 50.0" | bc -l) )); then
        COLOR=$YELLOW
    else
        COLOR=$RED
    fi
    
    printf "${BOLD}%-45s | ${COLOR}%6.2f%%${NC}${BOLD}     | %d/%d${NC}\n" "ОБЩЕЕ ПОКРЫТИЕ ПРОЕКТА" "$OVERALL_PERCENT" "$TOTAL_COVERED" "$TOTAL_LINES"
else
    printf "${BOLD}%-45s | ${RED}%6.2f%%${NC}${BOLD}     | 0/0${NC}\n" "ОБЩЕЕ ПОКРЫТИЕ ПРОЕКТА" "0.00"
fi
echo ""
