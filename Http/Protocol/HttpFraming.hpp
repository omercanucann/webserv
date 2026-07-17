#ifndef HTTPFRAMING_HPP
#define HTTPFRAMING_HPP

#include <string>
#include <vector>

class HttpFraming
{
    private:
        static std::string _getHeaderValue(const std::string &headerPart,
                                           const std::string &key);
    public:
        static bool findHeaderEnd(const std::vector<char> &buffer,
                                  size_t &headerEnd);
        static std::string toLower(const std::string &value);
        static size_t decimalSize(const std::string &value);
        static bool isRequestComplete(const std::vector<char> &buffer);
        static void reserveForContentLength(std::vector<char> &buffer);
};

#endif
