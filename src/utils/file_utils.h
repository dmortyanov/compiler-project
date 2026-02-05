#pragma once

#include <string>

namespace utils {
std::string read_file(const std::string& path);
bool write_file(const std::string& path, const std::string& content);
} // namespace utils
