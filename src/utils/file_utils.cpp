#include "utils/file_utils.h"

#include <fstream>
#include <sstream>

namespace utils {

std::string read_file(const std::string& path) {
    std::ifstream in(path, std::ios::in | std::ios::binary);
    if (!in) {
        return "";
    }
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

bool write_file(const std::string& path, const std::string& content) {
    std::ofstream out(path, std::ios::out | std::ios::binary);
    if (!out) {
        return false;
    }
    out << content;
    return true;
}

} // namespace utils
