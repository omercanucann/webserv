#ifndef FILEUTILS_HPP
#define FILEUTILS_HPP

#include <string>

namespace FileUtils
{
    bool createTempFile(const std::string &prefix, int &fd, std::string &path);
    bool writeAll(int fd, const char *data, size_t size);
}

#endif
