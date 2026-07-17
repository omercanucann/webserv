#include "FileUtils.hpp"
#include <cstdlib>
#include <cstdio>
#include <fcntl.h>
#include <fstream>
#include <sstream>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

bool FileUtils::createTempFile(const std::string &prefix,
                               int &fd,
                               std::string &path)
{
    std::string pattern;
    std::vector<char> buffer;

    pattern = "/tmp/webserv_" + prefix + "_XXXXXX";
    buffer.assign(pattern.begin(), pattern.end());
    buffer.push_back('\0');

    fd = ::mkstemp(&buffer[0]);
    if (fd < 0)
        return false;
    if (fcntl(fd, F_SETFD, FD_CLOEXEC) == -1)
    {
        close(fd);
        fd = -1;
        std::remove(&buffer[0]);
        return false;
    }
    path = &buffer[0];
    return true;
}

bool FileUtils::writeAll(int fd, const char *data, size_t size)
{
    size_t written;

    written = 0;
    while (written < size)
    {
        ssize_t n;

        n = write(fd, data + written, size - written);
        if (n > 0)
            written += static_cast<size_t>(n);
        else
            return false;
    }
    return true;
}

bool FileUtils::exists(const std::string &path)
{
    struct stat st;

    return stat(path.c_str(), &st) == 0;
}

bool FileUtils::isDirectory(const std::string &path)
{
    struct stat st;

    if (stat(path.c_str(), &st) != 0)
        return false;
    return S_ISDIR(st.st_mode);
}

bool FileUtils::readFile(const std::string &path, std::string &out)
{
    std::ifstream file;
    std::ostringstream content;

    file.open(path.c_str(), std::ios::in | std::ios::binary);
    if (!file.is_open())
        return false;

    content << file.rdbuf();
    if (file.bad())
    {
        file.close();
        return false;
    }

    file.close();
    out = content.str();
    return true;
}

std::string FileUtils::joinPath(const std::string &root,
                                const std::string &path)
{
    if (root.empty())
        return path;
    if (path.empty())
        return root;
    if (root[root.length() - 1] == '/' && path[0] == '/')
        return root + path.substr(1);
    if (root[root.length() - 1] != '/' && path[0] != '/')
        return root + "/" + path;
    return root + path;
}
