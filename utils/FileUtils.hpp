#ifndef FILEUTILS_HPP
#define FILEUTILS_HPP

#include <string>

namespace FileUtils
{
    bool createTempFile(const std::string &prefix, int &fd, std::string &path);
    bool writeAll(int fd, const char *data, size_t size);
    bool exists(const std::string &path);
    bool isDirectory(const std::string &path);
    bool readFile(const std::string &path, std::string &out);
    std::string joinPath(const std::string &root, const std::string &path);
}

#endif
