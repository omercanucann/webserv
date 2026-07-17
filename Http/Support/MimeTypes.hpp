#ifndef MIMETYPES_HPP
#define MIMETYPES_HPP

#include <string>
#include <map>

class MimeTypes
{
private:
    std::map<std::string, std::string> _types;

    std::string getExtension(const std::string& path) const;

public:
    MimeTypes();

    std::string getMimeType(const std::string& path) const;
};

#endif
