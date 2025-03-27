#pragma once

#include <cstring>
#include <sstream>
#include <string>
#include <vector>
#include <exception>
#include <type_traits>
#include <array>

namespace NCommon {

////////////////////////////////////////////////////////////////////////////////

template <typename T>
inline void FormatHandler(std::ostringstream& out, const T& value) {
    out << value;
}

template <typename T>
requires(std::is_base_of_v<std::exception, T>)
inline void FormatHandler(std::ostringstream& out, const T& value) {
    out << value.what();
}

struct errno_type { int error_num; };
#define Errno ::NCommon::errno_type{errno}

inline void FormatHandler(std::ostringstream& out, const errno_type& value) {
    out << std::strerror(value.error_num);
}

////////////////////////////////////////////////////////////////////////////////

namespace detail {

////////////////////////////////////////////////////////////////////////////////

inline void FormatImpl(std::ostringstream& result, 
                       const std::string& format, 
                       size_t& pos) {
    result << format.substr(pos);
}

template<typename T, typename... Rest>
void FormatImpl(std::ostringstream& result, 
                const std::string& format, 
                size_t& pos, 
                T&& value, 
                Rest&&... rest) {
    size_t next = format.find("{}", pos);
    if (next == std::string::npos) {
        result << format.substr(pos);
        return;
    }
    
    result << format.substr(pos, next - pos);
    ::NCommon::FormatHandler(result, value);
    pos = next + 2;
    FormatImpl(result, format, pos, std::forward<Rest>(rest)...);
}

static const auto& GetEscapeMap() {
    static std::array<char, 256> s_escape_map = []() {
        std::array<char, 256> map{};
        map['\\'] = '\\';
        map['\n'] = 'n';
        map['\r'] = 'r';
        return map;
    }();
    return s_escape_map;
}

////////////////////////////////////////////////////////////////////////////////

} // namespace detail

////////////////////////////////////////////////////////////////////////////////

std::string EscapeSymbols(const std::string& str);

std::vector<std::string> Split(const std::string& s, const std::string& delimiter, size_t limit = 0);

std::string Trim(const std::string& s);

template <typename TContainer>
std::string Join(const TContainer& elements, const std::string& delimiter = ", ") {
    if (elements.empty()) return "";
    
    std::ostringstream ss;
    for (const auto& el : elements) {
        if (&el != &*elements.begin()) {
            ss << delimiter;
        }
        ss << el;
    }
    return ss.str();
}

////////////////////////////////////////////////////////////////////////////////

template<typename... Args>
std::string Format(const std::string& format, Args&&... args) {
    std::ostringstream result;
    size_t pos = 0;
    detail::FormatImpl(result, format, pos, std::forward<Args>(args)...);
    return result.str();
}

////////////////////////////////////////////////////////////////////////////////

} // namespace NCommon
