#ifndef STRINGUTILS_HPP
#define STRINGUTILS_HPP

#include <string>

namespace StringUtils
{
    std::string toLowerAscii(const std::string &value);
    std::string trim(const std::string &value, const std::string &characters);
    std::string sizeToString(size_t value);
}

#endif
