#pragma once

#include <string>
#include <unordered_map>
#include <vector>

struct PreprocessError {
    int line;
    int column;
    std::string message;
};

class Preprocessor {
public:
    explicit Preprocessor(const std::string& source);

    std::string process();

    void define(const std::string& name, const std::string& value);
    void undefine(const std::string& name);

    const std::vector<PreprocessError>& errors() const;

private:
    std::string source_;
    std::unordered_map<std::string, std::string> macros_;
    std::vector<PreprocessError> errors_;
    std::vector<bool> active_stack_;

    bool is_active() const;
    void report_error(int line, int column, const std::string& message);

    bool is_identifier_start(char c) const;
    bool is_identifier_char(char c) const;

    std::string expand_text(const std::string& text,
                            std::vector<std::string>& expansion_stack,
                            int line, int column);
};
