#include "HttpRequest.hpp"
#include "../../utils/StringUtils.hpp"

HttpRequest::HttpRequest()
    : _bodySize(0),
      _bodyStoredInFile(false)
{
}

void HttpRequest::setMethod(const std::string& method)
{
    _method = method;
}

void HttpRequest::setPath(const std::string& path)
{
    _path = path;
}

void HttpRequest::setQuery(const std::string& query)
{
    _query = query;
}

void HttpRequest::setVersion(const std::string& version)
{
    _version = version;
}

void HttpRequest::setHeader(const std::string& key, const std::string& value)
{
    _headers[StringUtils::toLowerAscii(key)] = value;
}

void HttpRequest::setBody(const std::string& body)
{
    _body = body;
    _bodyFilePath.clear();
    _bodySize = body.size();
    _bodyStoredInFile = false;
}

void HttpRequest::setBodyFile(const std::string& path, size_t bodySize)
{
    _body.clear();
    _bodyFilePath = path;
    _bodySize = bodySize;
    _bodyStoredInFile = true;
}

const std::string& HttpRequest::getMethod() const
{
    return _method;
}

const std::string& HttpRequest::getPath() const
{
    return _path;
}

const std::string& HttpRequest::getQuery() const
{
    return _query;
}

const std::string& HttpRequest::getVersion() const
{
    return _version;
}

const std::string& HttpRequest::getBody() const
{
    return _body;
}

const std::string& HttpRequest::getBodyFilePath() const
{
    return _bodyFilePath;
}

size_t HttpRequest::getBodySize() const
{
    return _bodySize;
}

bool HttpRequest::hasBodyFile() const
{
    return _bodyStoredInFile;
}

std::string HttpRequest::getHeader(const std::string& key) const
{
    std::map<std::string, std::string>::const_iterator it;

    it = _headers.find(StringUtils::toLowerAscii(key));
    if (it == _headers.end())
        return "";
    return it->second;
}

bool HttpRequest::hasHeader(const std::string& key) const
{
    return _headers.find(StringUtils::toLowerAscii(key)) != _headers.end();
}
