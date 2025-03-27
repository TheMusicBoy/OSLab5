#include <common/format.h>

namespace NCommon {

////////////////////////////////////////////////////////////////////////////////

std::string EscapeSymbols(const std::string& str) {
    static const auto& escape = detail::GetEscapeMap(); 
    size_t count = 0;
    for (char c : str) {
        if (escape[static_cast<unsigned char>(c)]) {
            count++;
        }
    }

    std::string result(str.size() + count, '\\');
    size_t offset = 0;
    for (size_t pos = 0; pos < str.size(); pos++) {
        char esc = escape[static_cast<unsigned char>(str[pos])];
        if (esc) {
            result[pos + offset] = '\\';
            offset++;
            result[pos + offset] = esc;
        } else {
            result[pos + offset] = str[pos];
        }
    }

    return result;
}

std::vector<std::string> Split(const std::string& s, const std::string& delimiter, size_t limit) {
    std::vector<std::string> tokens;
    size_t pos_start = 0;
    const size_t delimiter_length = delimiter.length();

    if (delimiter_length == 0) {
        tokens.push_back(s);
        return tokens;
    }

    while (true) {
        size_t pos_end = s.find(delimiter, pos_start);
        bool reached_limit = (limit > 0) && (tokens.size() + 1 >= limit);

        if (reached_limit || pos_end == std::string::npos) {
            tokens.push_back(s.substr(pos_start));
            break;
        }

        tokens.push_back(s.substr(pos_start, pos_end - pos_start));
        pos_start = pos_end + delimiter_length;
    }

    return tokens;
}

std::string Join(const std::vector<std::string>& elements, const std::string& delimiter) {
    if (elements.empty()) return "";
    
    std::string result;
    for (size_t i = 0; i < elements.size() - 1; ++i) {
        result += elements[i];
        result += delimiter;
    }
    result += elements.back();
    return result;
}

std::string Trim(const std::string& s) {
    const char* whitespace = " \t\r\n";
    size_t start = s.find_first_not_of(whitespace);
    if (start == std::string::npos) return "";
    size_t end = s.find_last_not_of(whitespace);
    return s.substr(start, end - start + 1);
}

////////////////////////////////////////////////////////////////////////////////

} // namespace NCommon
