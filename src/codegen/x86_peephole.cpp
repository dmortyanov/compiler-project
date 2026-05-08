#include "codegen/x86_peephole.h"

#include <sstream>
#include <vector>
#include <algorithm>

// ---------------------------------------------------------------
// Вспомогательные функции
// ---------------------------------------------------------------

static std::string trim_ws(const std::string& s) {
    size_t start = s.find_first_not_of(" \t");
    if (start == std::string::npos) return "";
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

static bool is_instruction(const std::string& line) {
    std::string trimmed = trim_ws(line);
    if (trimmed.empty()) return false;
    if (trimmed[0] == ';') return false;      // комментарий
    if (trimmed.back() == ':') return false;   // метка
    if (trimmed.substr(0, 7) == "section") return false;
    if (trimmed.substr(0, 6) == "global") return false;
    if (trimmed.substr(0, 6) == "extern") return false;
    if (trimmed.substr(0, 2) == "db") return false;
    return true;
}

// Извлекает мнемонику и операнды из строки вида "    mov eax, ecx"
static bool parse_instruction(const std::string& line,
                               std::string& mnemonic,
                               std::string& op1,
                               std::string& op2) {
    std::string trimmed = trim_ws(line);
    // Убираем trailing comment ("; ...")
    auto semi = trimmed.find(';');
    if (semi != std::string::npos) {
        trimmed = trimmed.substr(0, semi);
        // re-trim
        size_t e = trimmed.find_last_not_of(" \t");
        if (e != std::string::npos) trimmed = trimmed.substr(0, e + 1);
    }

    // Найти первый пробел (после мнемоники)
    auto sp = trimmed.find(' ');
    if (sp == std::string::npos) {
        mnemonic = trimmed;
        op1 = "";
        op2 = "";
        return true;
    }

    mnemonic = trimmed.substr(0, sp);
    std::string rest = trim_ws(trimmed.substr(sp + 1));

    // Разделить по запятой
    auto comma = rest.find(',');
    if (comma == std::string::npos) {
        op1 = trim_ws(rest);
        op2 = "";
    } else {
        op1 = trim_ws(rest.substr(0, comma));
        op2 = trim_ws(rest.substr(comma + 1));
    }
    return true;
}

// Проверяет, является ли строка меткой и возвращает её имя
static bool is_label(const std::string& line, std::string& label_name) {
    std::string trimmed = trim_ws(line);
    if (!trimmed.empty() && trimmed.back() == ':') {
        label_name = trimmed.substr(0, trimmed.size() - 1);
        return true;
    }
    return false;
}

// ---------------------------------------------------------------
// optimize — главный метод оконной оптимизации
// ---------------------------------------------------------------
std::string X86Peephole::optimize(const std::string& asm_text) {
    removed_  = 0;
    replaced_ = 0;

    // Разбиваем текст на строки
    std::vector<std::string> lines;
    std::istringstream stream(asm_text);
    std::string line;
    while (std::getline(stream, line)) {
        lines.push_back(line);
    }

    // Маска: true = строка удалена
    std::vector<bool> deleted(lines.size(), false);

    for (size_t i = 0; i < lines.size(); ++i) {
        if (deleted[i]) continue;
        if (!is_instruction(lines[i])) continue;

        std::string m1, op1a, op1b;
        if (!parse_instruction(lines[i], m1, op1a, op1b)) continue;

        // ----- Паттерн 2: mov X, X (identity) -----
        if (m1 == "mov" && !op1a.empty() && op1a == op1b) {
            deleted[i] = true;
            removed_++;
            continue;
        }

        // ----- Паттерн 3: mov reg, 0 → xor reg, reg -----
        if (m1 == "mov" && op1b == "0" && !op1a.empty()) {
            // Только для чистых регистров (не memory operands)
            if (op1a.find('[') == std::string::npos) {
                // Заменяем на xor
                std::string indent = lines[i].substr(0, lines[i].find_first_not_of(" \t"));
                lines[i] = indent + "xor " + op1a + ", " + op1a;
                replaced_++;
                continue;
            }
        }

        // ----- Паттерн 4: add X, 0 / sub X, 0 → удалить -----
        if ((m1 == "add" || m1 == "sub") && op1b == "0") {
            deleted[i] = true;
            removed_++;
            continue;
        }

        // ----- Паттерн 5: imul X, 1 → удалить -----
        if (m1 == "imul" && op1b == "1") {
            deleted[i] = true;
            removed_++;
            continue;
        }

        // ----- Паттерн 6: jmp .Lnext → удалить если .Lnext = следующая метка -----
        if (m1 == "jmp" && !op1a.empty()) {
            // Ищем следующую не-пустую строку
            for (size_t j = i + 1; j < lines.size(); ++j) {
                std::string trimmed = trim_ws(lines[j]);
                if (trimmed.empty() || trimmed[0] == ';') continue;  // пропускаем пустые/комментарии

                std::string lbl;
                if (is_label(lines[j], lbl)) {
                    if (op1a == lbl) {
                        // jmp к следующей метке — избыточен
                        deleted[i] = true;
                        removed_++;
                    }
                }
                break;  // Проверяем только первую непустую строку после jmp
            }
            if (deleted[i]) continue;
        }

        // ----- Паттерн 1: mov X, Y; mov Y, X → удалить вторую -----
        if (m1 == "mov" && !op1a.empty() && !op1b.empty()) {
            // Ищем следующую инструкцию
            for (size_t j = i + 1; j < lines.size(); ++j) {
                if (deleted[j]) continue;
                std::string trimmed = trim_ws(lines[j]);
                if (trimmed.empty() || trimmed[0] == ';') continue;
                if (!is_instruction(lines[j])) break;

                std::string m2, op2a, op2b;
                if (parse_instruction(lines[j], m2, op2a, op2b)) {
                    if (m2 == "mov" && op2a == op1b && op2b == op1a) {
                        deleted[j] = true;
                        removed_++;
                    }
                }
                break;
            }
        }
    }

    // Собираем результат
    std::ostringstream result;
    for (size_t i = 0; i < lines.size(); ++i) {
        if (!deleted[i]) {
            result << lines[i] << "\n";
        }
    }
    return result.str();
}

// ---------------------------------------------------------------
// report — отчёт об оптимизациях
// ---------------------------------------------------------------
std::string X86Peephole::report() const {
    std::ostringstream out;
    out << "=== x86 Peephole Optimization ===\n";
    out << "Instructions removed:  " << removed_  << "\n";
    out << "Instructions replaced: " << replaced_ << "\n";
    out << "Total optimized:       " << (removed_ + replaced_) << "\n";
    return out.str();
}
