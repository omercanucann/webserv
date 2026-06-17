#include "HttpRequestHandler.hpp"
#include "DefaultConfig.hpp"
#include <iostream>
#include <sstream>
#include <cstdlib>
#include <sys/stat.h>
#include <sys/types.h>
#include <fstream>
#include <ctime>
#include <cstdio>

HttpRequestHandler::HttpRequestHandler(const Config &config) : _router(config), _staticHandler()
{
}

HttpRequestHandler::~HttpRequestHandler()
{
}

std::string HttpRequestHandler::_sizeToString(size_t value) const
{
    std::ostringstream oss;

    oss << value;
    return oss.str();
}

HttpResponse HttpRequestHandler::_makeErrorResponse(int statusCode) const
{
    HttpResponse response;
    std::string filePath;
    std::string body;

    filePath = DefaultConfig::ERROR_ROOT + "/";
    filePath += _sizeToString(static_cast<size_t>(statusCode));
    filePath += ".html";

    if (_fileExists(filePath) && !_isDirectory(filePath))
    {
        if (_readFile(filePath, body))
        {
            response.setStatus(statusCode);
            response.setHeader("Content-Type", "text/html");
            response.setBody(body);
            return response;
        }
    }

    return HttpResponse::makeErrorResponse(statusCode);
}

std::string HttpRequestHandler::_toLower(const std::string &str) const
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

size_t HttpRequestHandler::_stringToSize(const std::string &str) const
{
    size_t result;
    size_t i;

    result = 0;
    i = 0;
    while (i < str.length())
    {
        if (str[i] < '0' || str[i] > '9')
            return 0;
        result = result * 10 + static_cast<size_t>(str[i] - '0');
        i++;
    }
    return result;
}

bool HttpRequestHandler::_hasHeaderEnd(const std::string &rawRequest, size_t &headerEnd) const
{
    headerEnd = rawRequest.find("\r\n\r\n");
    return headerEnd != std::string::npos;
}

std::string HttpRequestHandler::_getHeaderValue(const std::string &headerPart,
                                                const std::string &key) const
{
    std::istringstream stream(headerPart);
    std::string line;
    std::string lowerKey;
    size_t colonPos;
    std::string headerName;
    std::string headerValue;

    lowerKey = _toLower(key);

    while (std::getline(stream, line))
    {
        if (!line.empty() && line[line.length() - 1] == '\r')
            line.erase(line.length() - 1);

        colonPos = line.find(':');
        if (colonPos == std::string::npos)
            continue;

        headerName = _toLower(line.substr(0, colonPos));
        headerValue = line.substr(colonPos + 1);

        while (!headerValue.empty()
            && (headerValue[0] == ' ' || headerValue[0] == '\t'))
            headerValue.erase(0, 1);

        while (!headerValue.empty()
            && (headerValue[headerValue.length() - 1] == ' '
                || headerValue[headerValue.length() - 1] == '\t'
                || headerValue[headerValue.length() - 1] == '\r'))
            headerValue.erase(headerValue.length() - 1);

        if (headerName == lowerKey)
            return headerValue;
    }

    return "";
}

bool HttpRequestHandler::_isRequestComplete(const std::string &rawRequest) const
{
    size_t headerEnd;
    std::string headerPart;
    std::string bodyPart;
    std::string contentLength;
    std::string transferEncoding;
    size_t expectedBodySize;

    if (!_hasHeaderEnd(rawRequest, headerEnd))
        return false;

    headerPart = rawRequest.substr(0, headerEnd);
    bodyPart = rawRequest.substr(headerEnd + 4);

    transferEncoding = _toLower(_getHeaderValue(headerPart, "Transfer-Encoding"));
    if (transferEncoding.find("chunked") != std::string::npos)
    {
        if (bodyPart.find("\r\n0\r\n\r\n") != std::string::npos)
            return true;
        if (bodyPart.find("0\r\n\r\n") == 0)
            return true;
        return false;
    }

    contentLength = _getHeaderValue(headerPart, "Content-Length");
    if (!contentLength.empty())
    {
        expectedBodySize = _stringToSize(contentLength);
        return bodyPart.length() >= expectedBodySize;
    }

    return true;
}

bool HttpRequestHandler::_readFile(const std::string &path, std::string &out) const
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

bool HttpRequestHandler::_isDirectory(const std::string &path) const
{
    struct stat st;

    if (stat(path.c_str(), &st) != 0)
        return false;

    return S_ISDIR(st.st_mode);
}

bool HttpRequestHandler::_fileExists(const std::string &path) const
{
    struct stat st;

    if (stat(path.c_str(), &st) != 0)
        return false;

    return true;
}

HttpResponse HttpRequestHandler::_buildResponse(const HttpRequest &request)
{
    RouteResult route = _router.route(request);

    if (route.server == NULL)
        return _makeErrorResponse(500);
    
    if (route.location == NULL)
        return _makeErrorResponse(404);

    if (request.getMethod() == "GET" || request.getMethod() == "DELETE" || request.getMethod() == "POST")
        return _staticHandler.handle(request, route);

    return _makeErrorResponse(405);
}

bool HttpRequestHandler::handle_data(int slot_index, ConnectionSlot &slot)
{
    std::string rawRequest;
    HttpRequest request;
    HttpResponse response;
    std::string rawResponse;

    (void)slot_index;

    rawRequest.assign(slot.readbuffer.begin(), slot.readbuffer.end());

    if (!_isRequestComplete(rawRequest))
        return false;

    try
    {
        request = _parser.parse(rawRequest);
        response = _buildResponse(request);
    }
    catch (const HttpParser::ParseException &e)
    {
        response = _makeErrorResponse(e.getStatusCode());
    }
    catch (const std::exception &)
    {
        response = _makeErrorResponse(500);
    }

    rawResponse = response.toString();

    slot.writebuffer.assign(rawResponse.begin(), rawResponse.end());
    slot.write_position = 0;
    slot.request_complete = true;
    slot.state = ConnectState_WRITING;

    return true;
}

void HttpRequestHandler::handle_close(int slot_index)
{
    std::cout << "[HttpRequestHandler] connection closed: slot["
              << slot_index << "]" << std::endl;
}
