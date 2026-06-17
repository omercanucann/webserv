#ifndef AUTOINDEX_HPP
# define AUTOINDEX_HPP

#include "HttpResponse.hpp"
#include <string>
#include <dirent.h>
#include <sys/stat.h>
#include <sstream>

class AutoIndex
{
    private:
        std::string _escapeHtml(const std::string &str) const;
        std::string _ensureSlashEnd(const std::string &path) const;
    public:
        AutoIndex();
        HttpResponse generate(const std::string &directoryPath, const std::string &requestPath) const;
};

#endif