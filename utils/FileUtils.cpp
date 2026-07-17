#include "FileUtils.hpp"
#include <cstdio>
#include <fcntl.h>
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

    fd = mkstemp(&buffer[0]);
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
