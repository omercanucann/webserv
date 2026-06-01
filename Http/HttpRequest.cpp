#include "HttpRequest.hpp"

HttpRequest::HttpRequest()
{
}

HttpRequest::~HttpRequest()
{
}

std::string HttpRequest::toLower(const std::string& str) const
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
    _headers[toLower(key)] = value;
}

void HttpRequest::setBody(const std::string& body)
{
    _body = body;
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

std::string HttpRequest::getHeader(const std::string& key) const
{
    std::map<std::string, std::string>::const_iterator it;

    it = _headers.find(toLower(key));
    if (it == _headers.end())
        return "";
    return it->second;
}

bool HttpRequest::hasHeader(const std::string& key) const
{
    return _headers.find(toLower(key)) != _headers.end();
}

void HttpRequest::clear()
{
    _method.clear();
    _path.clear();
    _query.clear();
    _version.clear();
    _headers.clear();
    _body.clear();
}