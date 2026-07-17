#include "HttpResponse.hpp"
#include "StatusCode.hpp"
#include <sstream>

HttpResponse::HttpResponse()
    : _version("HTTP/1.1"), _statusCode(200), _reasonPhrase("OK")
{
}

std::string HttpResponse::toLower(const std::string& str) const
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

std::string HttpResponse::intToString(size_t number) const
{
    std::ostringstream stream;

    stream << number;
    return stream.str();
}

void HttpResponse::setVersion(const std::string& version)
{
    _version = version;
}

void HttpResponse::setStatus(int statusCode)
{
    _statusCode = statusCode;
    _reasonPhrase = StatusCode::getReasonPhrase(statusCode);
}

void HttpResponse::setHeader(const std::string& key, const std::string& value)
{
    _headers[key] = value;
}

void HttpResponse::setBody(const std::string& body)
{
    _body = body;
}

const std::string& HttpResponse::getVersion() const
{
    return _version;
}

int HttpResponse::getStatusCode() const
{
    return _statusCode;
}

const std::string& HttpResponse::getReasonPhrase() const
{
    return _reasonPhrase;
}

const std::string& HttpResponse::getBody() const
{
    return _body;
}

std::string HttpResponse::getHeader(const std::string& key) const
{
    std::map<std::string, std::string>::const_iterator current;
    std::string lowerKey;

    lowerKey = toLower(key);
    current = _headers.begin();
    while (current != _headers.end())
    {
        if (toLower(current->first) == lowerKey)
            return current->second;
        current++;
    }
    return "";
}

bool HttpResponse::hasHeader(const std::string& key) const
{
    std::map<std::string, std::string>::const_iterator current;
    std::string lowerKey;

    lowerKey = toLower(key);
    current = _headers.begin();
    while (current != _headers.end())
    {
        if (toLower(current->first) == lowerKey)
            return true;
        current++;
    }
    return false;
}

std::string HttpResponse::toString()
{
    std::ostringstream response;
    std::map<std::string, std::string>::const_iterator it;

    if (!StatusCode::isBodyAllowed(_statusCode))
        _body.clear();
    if (!hasHeader("Content-Length"))
        setHeader("Content-Length", intToString(_body.length()));
    if (!hasHeader("Connection"))
        setHeader("Connection", "close");

    response << _version << " " << _statusCode << " " << _reasonPhrase << "\r\n";

    it = _headers.begin();
    while (it != _headers.end())
    {
        response << it->first << ": " << it->second << "\r\n";
        it++;
    }

    response << "\r\n";
    if (_statusCode != 204)
        response << _body;

    return response.str();
}

HttpResponse HttpResponse::makeErrorResponse(int statusCode)
{
    HttpResponse response;
    std::string reason;
    std::string body;
    std::ostringstream codeStream;

    response.setStatus(statusCode);
    reason = response.getReasonPhrase();

    codeStream << statusCode;

    body = "<html>\n";
    body += "<head><title>";
    body += codeStream.str();
    body += " ";
    body += reason;
    body += "</title></head>\n";
    body += "<body>\n";
    body += "<h1>";
    body += codeStream.str();
    body += " ";
    body += reason;
    body += "</h1>\n";
    body += "</body>\n";
    body += "</html>\n";

    response.setHeader("Content-Type", "text/html");
    response.setBody(body);

    return response;
}

HttpResponse HttpResponse::makeRedirectResponse(int statusCode, const std::string& location)
{
    HttpResponse response;
    std::string body;

    response.setStatus(statusCode);
    response.setHeader("Location", location);
    response.setHeader("Content-Type", "text/html");

    body = "<html>\n";
    body += "<head><title>";
    body += response.getReasonPhrase();
    body += "</title></head>\n";
    body += "<body>\n";
    body += "<h1>";
    body += response.getReasonPhrase();
    body += "</h1>\n";
    body += "<p>Redirecting to ";
    body += location;
    body += "</p>\n";
    body += "</body>\n";
    body += "</html>\n";

    response.setBody(body);

    return response;
}

HttpResponse HttpResponse::makeMethodNotAllowedResponse(const std::string& allowedMethods)
{
    HttpResponse response;
    std::string body;

    response.setStatus(405);
    response.setHeader("Allow", allowedMethods);
    response.setHeader("Content-Type", "text/html");

    body = "<html>\n";
    body += "<head><title>405 Method Not Allowed</title></head>\n";
    body += "<body>\n";
    body += "<h1>405 Method Not Allowed</h1>\n";
    body += "</body>\n";
    body += "</html>\n";

    response.setBody(body);

    return response;
}
