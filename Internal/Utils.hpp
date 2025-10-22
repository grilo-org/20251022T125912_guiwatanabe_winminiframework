#pragma once

#include <string>
#include <sstream>

namespace Utils
{
    static std::string urlDecode(const std::string& s) {
        std::string result;
        for (size_t i = 0; i < s.length(); ++i) {
            if (s[i] == '%' && i + 2 < s.length() && isxdigit(s[i + 1]) && isxdigit(s[i + 2])) {
                int value;
                std::stringstream ss;
                ss << std::hex << s.substr(i + 1, 2);
                ss >> value;
                result += static_cast<char>(value);
                i += 2;
            }
            else if (s[i] == '+') {
                result += ' ';
            }
            else {
                result += s[i];
            }
        }
        return result;
    }
}