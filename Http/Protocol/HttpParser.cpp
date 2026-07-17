#include "HttpParser.hpp"
#include <sstream>

HttpParser::ParseException::ParseException(int statusCode, const std::string& message)
    : _statusCode(statusCode), _message(message)
{
}

HttpParser::ParseException::~ParseException() throw()
{
}

int HttpParser::ParseException::getStatusCode() const
{
    return _statusCode;
}

const char* HttpParser::ParseException::what() const throw()
{
    return _message.c_str();
}

HttpParser::HttpParser()
    : _maxBodySize(1024 * 1024)
{
}

HttpParser::HttpParser(size_t maxBodySize)
    : _maxBodySize(maxBodySize)
{
}

std::string HttpParser::trim(const std::string& str) const
{
    size_t start;
    size_t end;

    start = str.find_first_not_of(" \t\r\n");
    if (start == std::string::npos)
        return "";

    end = str.find_last_not_of(" \t\r\n");
    return str.substr(start, end - start + 1);
}

std::string HttpParser::toLower(const std::string& str) const
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

bool HttpParser::isTokenChar(char c) const
{
    if (c >= 'A' && c <= 'Z')
        return true;
    if (c >= 'a' && c <= 'z')
        return true;
    if (c >= '0' && c <= '9')
        return true;

    return c == '!' || c == '#' || c == '$' || c == '%' ||
           c == '&' || c == '\'' || c == '*' || c == '+' ||
           c == '-' || c == '.' || c == '^' || c == '_' ||
           c == '`' || c == '|' || c == '~';
}

bool HttpParser::isValidMethodToken(const std::string& method) const
{
    size_t i;

    if (method.empty())
        return false;

    i = 0;
    while (i < method.length())
    {
        if (!isTokenChar(method[i]))
            return false;
        i++;
    }
    return true;
}

bool HttpParser::isValidHeaderName(const std::string& key) const
{
    size_t i;

    if (key.empty())
        return false;

    i = 0;
    while (i < key.length())
    {
        if (!isTokenChar(key[i]))
            return false;
        i++;
    }
    return true;
}

bool HttpParser::isOnlyDigits(const std::string& str) const
{
    size_t i;

    if (str.empty())
        return false;

    i = 0;
    while (i < str.length())
    {
        if (str[i] < '0' || str[i] > '9')
            return false;
        i++;
    }
    return true;
}

size_t HttpParser::stringToSize(const std::string& str) const
{
    size_t result;
    size_t i;

    if (!isOnlyDigits(str))
        throw ParseException(400, "Invalid Content-Length");

    result = 0;
    i = 0;
    while (i < str.length())
    {
        if (result > (static_cast<size_t>(-1) - static_cast<size_t>(str[i] - '0')) / 10)
            throw ParseException(413, "Content-Length too large");

        result = result * 10 + static_cast<size_t>(str[i] - '0');
        i++;
    }
    return result;
}

size_t HttpParser::hexToSize(const std::string& str) const
{
    size_t result;
    size_t i;
    char c;
    int value;

    if (str.empty())
        throw ParseException(400, "Invalid chunk size");

    result = 0;
    i = 0;
    while (i < str.length())
    {
        c = str[i];

        if (c >= '0' && c <= '9')
            value = c - '0';
        else if (c >= 'a' && c <= 'f')
            value = c - 'a' + 10;
        else if (c >= 'A' && c <= 'F')
            value = c - 'A' + 10;
        else
            throw ParseException(400, "Invalid chunk size");

        if (result > (static_cast<size_t>(-1) - static_cast<size_t>(value)) / 16)
            throw ParseException(413, "Chunk size too large");

        result = result * 16 + static_cast<size_t>(value);
        i++;
    }
    return result;
}

void HttpParser::parsePathAndQuery(const std::string& target, HttpRequest& request) const
{
    size_t questionPos;
    std::string path;
    std::string query;

    if (target.empty())
        throw ParseException(400, "Empty request target");

    if (target[0] != '/')
        throw ParseException(400, "Request target must start with /");

    questionPos = target.find('?');
    if (questionPos == std::string::npos)
    {
        path = target;
        query = "";
    }
    else
    {
        path = target.substr(0, questionPos);
        query = target.substr(questionPos + 1);
    }

    if (path.empty())
        path = "/";

    request.setPath(path);
    request.setQuery(query);
}

void HttpParser::parseRequestLine(const std::string& line, HttpRequest& request) const
{
    std::istringstream stream(line);
    std::string method;
    std::string target;
    std::string version;
    std::string extra;

    if (line.empty())
        throw ParseException(400, "Empty request line");

    stream >> method >> target >> version;

    if (method.empty() || target.empty() || version.empty())
        throw ParseException(400, "Malformed request line");

    if (stream >> extra)
        throw ParseException(400, "Too many parts in request line");

    if (!isValidMethodToken(method))
        throw ParseException(400, "Invalid method token");

    if (version != "HTTP/1.1" && version != "HTTP/1.0")
        throw ParseException(505, "HTTP version not supported");

    request.setMethod(method);
    parsePathAndQuery(target, request);
    request.setVersion(version);
}

void HttpParser::parseHeaderLine(const std::string& line, HttpRequest& request) const
{
    size_t colonPos;
    std::string key;
    std::string value;

    if (line.empty())
        return;

    colonPos = line.find(':');
    if (colonPos == std::string::npos)
        throw ParseException(400, "Malformed header line");

    key = trim(line.substr(0, colonPos));
    value = trim(line.substr(colonPos + 1));

    if (!isValidHeaderName(key))
        throw ParseException(400, "Invalid header name");

    request.setHeader(key, value);
}

std::string HttpParser::decodeChunkedBody(const std::string& body) const
{
    std::string result;
    size_t pos;
    size_t lineEnd;
    size_t chunkSize;
    std::string sizeLine;
    size_t semicolonPos;

    pos = 0;

    while (true)
    {
        lineEnd = body.find("\r\n", pos);
        if (lineEnd == std::string::npos)
            throw ParseException(400, "Malformed chunked body");

        sizeLine = body.substr(pos, lineEnd - pos);

        semicolonPos = sizeLine.find(';');
        if (semicolonPos != std::string::npos)
            sizeLine = sizeLine.substr(0, semicolonPos);

        sizeLine = trim(sizeLine);
        chunkSize = hexToSize(sizeLine);

        pos = lineEnd + 2;

        if (chunkSize == 0)
        {
            if (body.substr(pos, 2) != "\r\n")
                throw ParseException(400, "Malformed final chunk");
            break;
        }

        if (pos + chunkSize > body.length())
            throw ParseException(400, "Incomplete chunk data");

        result.append(body, pos, chunkSize);
        pos += chunkSize;

        if (body.substr(pos, 2) != "\r\n")
            throw ParseException(400, "Missing CRLF after chunk data");

        pos += 2;

        if (result.length() > _maxBodySize)
            throw ParseException(413, "Request body too large");
    }

    return result;
}

void HttpParser::validateBodyRules(HttpRequest& request, const std::string& rawBody) const
{
    bool hasContentLength;
    bool isChunked;
    size_t contentLength;
    std::string transferEncoding;
    std::string body;

    hasContentLength = request.hasHeader("Content-Length");
    transferEncoding = toLower(request.getHeader("Transfer-Encoding"));
    isChunked = transferEncoding.find("chunked") != std::string::npos;

    if (hasContentLength && isChunked)
        throw ParseException(400, "Both Content-Length and Transfer-Encoding are present");

    if (isChunked)
    {
        body = decodeChunkedBody(rawBody);
        request.setBody(body);
        return;
    }

    if (hasContentLength)
    {
        contentLength = stringToSize(trim(request.getHeader("Content-Length")));

        if (contentLength > _maxBodySize)
            throw ParseException(413, "Request body too large");

        if (rawBody.length() < contentLength)
            throw ParseException(400, "Incomplete request body");

        request.setBody(rawBody.substr(0, contentLength));
        return;
    }

    if (request.getMethod() == "POST" && !rawBody.empty())
        throw ParseException(411, "Length Required");

    request.setBody("");
}

HttpRequest HttpParser::parseHeaders(const std::string& rawRequest) const
{
    HttpRequest request;
    size_t headerEnd;
    std::string headerPart;
    std::istringstream stream;
    std::string line;
    bool firstLine;

    if (rawRequest.empty())
        throw ParseException(400, "Empty request");

    headerEnd = rawRequest.find("\r\n\r\n");
    if (headerEnd == std::string::npos)
        throw ParseException(400, "Header section is not complete");

    headerPart = rawRequest.substr(0, headerEnd);

    stream.str(headerPart);
    firstLine = true;

    while (std::getline(stream, line))
    {
        if (!line.empty() && line[line.length() - 1] == '\r')
            line.erase(line.length() - 1);

        if (firstLine)
        {
            parseRequestLine(line, request);
            firstLine = false;
        }
        else
        {
            parseHeaderLine(line, request);
        }
    }

    if (firstLine)
        throw ParseException(400, "Missing request line");

    if (request.getVersion() == "HTTP/1.1" && !request.hasHeader("Host"))
        throw ParseException(400, "Missing Host header");

    return request;
}

HttpRequest HttpParser::parse(const std::string& rawRequest) const
{
    HttpRequest request;
    size_t headerEnd;
    std::string bodyPart;

    request = parseHeaders(rawRequest);
    headerEnd = rawRequest.find("\r\n\r\n");
    bodyPart = rawRequest.substr(headerEnd + 4);
    validateBodyRules(request, bodyPart);

    return request;
}
