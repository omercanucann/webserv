#include "MimeTypes.hpp"

MimeTypes::MimeTypes()
{
    _types["html"] = "text/html";
    _types["htm"] = "text/html";
    _types["css"] = "text/css";
    _types["js"] = "application/javascript";
    _types["json"] = "application/json";
    _types["txt"] = "text/plain";

    _types["png"] = "image/png";
    _types["jpg"] = "image/jpeg";
    _types["jpeg"] = "image/jpeg";
    _types["gif"] = "image/gif";
    _types["svg"] = "image/svg+xml";
    _types["ico"] = "image/x-icon";

    _types["pdf"] = "application/pdf";
    _types["zip"] = "application/zip";
    _types["mp3"] = "audio/mpeg";
    _types["mp4"] = "video/mp4";
}

MimeTypes::~MimeTypes()
{
}

std::string MimeTypes::toLower(const std::string& str) const
{
    std::string result;
    size_t i;

    result = str;
    i = 0;
    while (i < result.length())
    {
        if (result[i] >= 'A' && result[i] <= 'Z')
            result[i] = result[i] + 32;
        i++;
    }
    return result;
}

std::string MimeTypes::getExtension(const std::string& path) const
{
    size_t dotPos;
    size_t slashPos;

    dotPos = path.find_last_of('.');
    slashPos = path.find_last_of('/');

    if (dotPos == std::string::npos)
        return "";

    if (slashPos != std::string::npos && dotPos < slashPos)
        return "";

    if (dotPos == path.length() - 1)
        return "";

    return toLower(path.substr(dotPos + 1));
}

std::string MimeTypes::getMimeType(const std::string& path) const
{
    std::string extension;
    std::map<std::string, std::string>::const_iterator it;

    extension = getExtension(path);

    if (extension.empty())
        return "application/octet-stream";

    it = _types.find(extension);
    if (it == _types.end())
        return "application/octet-stream";

    return it->second;
}