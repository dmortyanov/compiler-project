#include "preprocessor/preprocessor.h"

#include <cctype>
#include <sstream>

Preprocessor::Preprocessor(const std::string& source) : source_(source) {}

void Preprocessor::define(const std::string& name,
                           const std::string& value) {
    macros_[name] = value;
}

void Preprocessor::undefine(const std::string& name) { macros_.erase(name); }

const std::vector<PreprocessError>& Preprocessor::errors() const {
    return errors_;
}

bool Preprocessor::is_active() const {
    if (active_stack_.empty()) {
        return true;
    }
    return active_stack_.back();
}

void Preprocessor::report_error(int line, int column,
                                const std::string& message) {
    errors_.push_back(PreprocessError{line, column, message});
}

bool Preprocessor::is_identifier_start(char c) const {
    return std::isalpha(static_cast<unsigned char>(c)) != 0;
}

bool Preprocessor::is_identifier_char(char c) const {
    return std::isalnum(static_cast<unsigned char>(c)) != 0 || c == '_';
}

std::string Preprocessor::expand_text(
    const std::string& text, std::vector<std::string>& expansion_stack,
    int line, int column) {
    std::string out;
    out.reserve(text.size());
    for (std::size_t i = 0; i < text.size();) {
        char c = text[i];
        if (is_identifier_start(c)) {
            std::size_t start = i;
            ++i;
            while (i < text.size() && is_identifier_char(text[i])) {
                ++i;
            }
            std::string name = text.substr(start, i - start);
            auto it = macros_.find(name);
            if (it == macros_.end()) {
                out += name;
                continue;
            }
            bool recursion = false;
            for (const auto& entry : expansion_stack) {
                if (entry == name) {
                    recursion = true;
                    break;
                }
            }
            if (recursion) {
                report_error(line, column + static_cast<int>(start),
                             "Macro recursion detected: " + name);
                out += name;
                continue;
            }
            expansion_stack.push_back(name);
            out += expand_text(it->second, expansion_stack, line, column);
            expansion_stack.pop_back();
            continue;
        }
        out += c;
        ++i;
    }
    return out;
}

std::string Preprocessor::process() {
    std::string output;
    output.reserve(source_.size());

    bool in_block_comment = false;
    bool in_string = false;
    int block_comment_line = 0;
    int block_comment_col = 0;

    std::size_t i = 0;
    int line = 1;
    while (i < source_.size()) {
        std::size_t line_end = i;
        while (line_end < source_.size() && source_[line_end] != '\n' &&
               source_[line_end] != '\r') {
            ++line_end;
        }
        std::string line_text = source_.substr(i, line_end - i);

        std::string newline;
        if (line_end < source_.size() && source_[line_end] == '\r') {
            if (line_end + 1 < source_.size() && source_[line_end + 1] == '\n') {
                newline = "\r\n";
                line_end += 2;
            } else {
                newline = "\r";
                line_end += 1;
            }
        } else if (line_end < source_.size() && source_[line_end] == '\n') {
            newline = "\n";
            line_end += 1;
        }

        bool only_ws = true;
        std::size_t hash_pos = std::string::npos;
        for (std::size_t k = 0; k < line_text.size(); ++k) {
            char c = line_text[k];
            if (c == ' ' || c == '\t') {
                continue;
            }
            only_ws = false;
            if (c == '#') {
                hash_pos = k;
            }
            break;
        }

        bool is_directive = (!in_block_comment && hash_pos != std::string::npos);
        if (is_directive) {
            std::string directive = line_text.substr(hash_pos + 1);
            std::istringstream iss(directive);
            std::string cmd;
            iss >> cmd;

            bool handled_directive = false;
            if (cmd == "ifdef" || cmd == "ifndef") {
                std::string name;
                iss >> name;
                bool parent_active = is_active();
                bool defined = (macros_.find(name) != macros_.end());
                bool cond = (cmd == "ifdef") ? defined : !defined;
                active_stack_.push_back(parent_active && cond);
                handled_directive = true;
            } else if (cmd == "endif") {
                if (!active_stack_.empty()) {
                    active_stack_.pop_back();
                } else {
                    report_error(line, 1, "Unmatched #endif");
                }
                handled_directive = true;
            } else if (cmd == "define" && is_active()) {
                std::string name;
                iss >> name;
                std::string value;
                std::getline(iss, value);
                if (!value.empty() && value[0] == ' ') {
                    value.erase(0, 1);
                }
                if (!name.empty()) {
                    define(name, value);
                }
                handled_directive = true;
            } else if (cmd == "undef" && is_active()) {
                std::string name;
                iss >> name;
                if (!name.empty()) {
                    undefine(name);
                }
                handled_directive = true;
            }

            if (handled_directive) {
                output += std::string(line_text.size(), ' ');
                output += newline;
                i = line_end;
                line++;
                continue;
            }

            if (!is_active()) {
                output += std::string(line_text.size(), ' ');
                output += newline;
                i = line_end;
                line++;
                continue;
            }
        }

        if (!is_active()) {
            output += std::string(line_text.size(), ' ');
            output += newline;
            i = line_end;
            line++;
            continue;
        }

        std::string processed;
        processed.reserve(line_text.size());
        std::vector<std::string> expansion_stack;

        for (std::size_t j = 0; j < line_text.size(); ++j) {
            char c = line_text[j];

            if (in_block_comment) {
                if (c == '*' && j + 1 < line_text.size() &&
                    line_text[j + 1] == '/') {
                    processed += "  ";
                    ++j;
                    in_block_comment = false;
                } else {
                    processed += ' ';
                }
                continue;
            }

            if (in_string) {
                processed += c;
                if (c == '\\' && j + 1 < line_text.size()) {
                    processed += line_text[++j];
                    continue;
                }
                if (c == '"') {
                    in_string = false;
                }
                continue;
            }

            if (c == '"' && !in_block_comment) {
                in_string = true;
                processed += c;
                continue;
            }

            if (c == '/' && j + 1 < line_text.size() &&
                line_text[j + 1] == '/') {
                processed += std::string(line_text.size() - j, ' ');
                break;
            }
            if (c == '/' && j + 1 < line_text.size() &&
                line_text[j + 1] == '*') {
                if (!in_block_comment) {
                    in_block_comment = true;
                    block_comment_line = line;
                    block_comment_col = static_cast<int>(j) + 1;
                }
                processed += "  ";
                ++j;
                continue;
            }

            if (is_identifier_start(c)) {
                std::size_t start = j;
                std::size_t end = j + 1;
                while (end < line_text.size() &&
                       is_identifier_char(line_text[end])) {
                    ++end;
                }
                std::string name = line_text.substr(start, end - start);
                auto it = macros_.find(name);
                if (it == macros_.end()) {
                    processed += name;
                } else {
                    processed +=
                        expand_text(it->second, expansion_stack, line,
                                    static_cast<int>(start) + 1);
                }
                j = end - 1;
                continue;
            }

            processed += c;
        }

        output += processed;
        output += newline;
        i = line_end;
        line++;
    }

    if (in_block_comment) {
        report_error(block_comment_line, block_comment_col,
                     "Unterminated multi-line comment");
    }
    if (!active_stack_.empty()) {
        report_error(line, 1, "Unterminated conditional block");
    }

    return output;
}
