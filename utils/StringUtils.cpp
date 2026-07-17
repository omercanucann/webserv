#include "StringUtils.hpp"
#include <sstream>

std::string StringUtils::toLowerAscii(const std::string &value)
{
    std::string result;
    size_t i;

    result = value;
    i = 0;
    while (i < result.length())
    {
        if (result[i] >= 'A' && result[i] <= 'Z')
            result[i] = result[i] + 32;
        i++;
    }
    return result;
}

std::string StringUtils::trim(const std::string &value,
                              const std::string &characters)
{
    size_t start;
    size_t end;

    start = value.find_first_not_of(characters);
    if (start == std::string::npos)
        return "";

    end = value.find_last_not_of(characters);
    return value.substr(start, end - start + 1);
}

std::string StringUtils::sizeToString(size_t value)
{
    std::ostringstream stream;

    stream << value;
    return stream.str();
}
