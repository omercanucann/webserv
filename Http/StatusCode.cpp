#include "StatusCode.hpp"

std::string StatusCode::getReasonPhrase(int statusCode)
{
    if (statusCode == 200)
        return "OK";
    if (statusCode == 201)
        return "Created";
    if (statusCode == 204)
        return "No Content";

    if (statusCode == 301)
        return "Moved Permanently";
    if (statusCode == 302)
        return "Found";
    if (statusCode == 303)
        return "See Other";
    if (statusCode == 307)
        return "Temporary Redirect";
    if (statusCode == 308)
        return "Permanent Redirect";

    if (statusCode == 400)
        return "Bad Request";
    if (statusCode == 403)
        return "Forbidden";
    if (statusCode == 404)
        return "Not Found";
    if (statusCode == 405)
        return "Method Not Allowed";
    if (statusCode == 408)
        return "Request Timeout";
    if (statusCode == 411)
        return "Length Required";
    if (statusCode == 413)
        return "Payload Too Large";
    if (statusCode == 414)
        return "URI Too Long";
    if (statusCode == 415)
        return "Unsupported Media Type";

    if (statusCode == 500)
        return "Internal Server Error";
    if (statusCode == 501)
        return "Not Implemented";
    if (statusCode == 502)
        return "Bad Gateway";
    if (statusCode == 503)
        return "Service Unavailable";
    if (statusCode == 504)
        return "Gateway Timeout";
    if (statusCode == 505)
        return "HTTP Version Not Supported";

    return "Unknown";
}

bool StatusCode::isBodyAllowed(int statusCode)
{
    if (statusCode == 204)
        return false;
    if (statusCode == 304)
        return false;
    return true;
}

bool StatusCode::isRedirect(int statusCode)
{
    return statusCode == 301 ||
           statusCode == 302 ||
           statusCode == 303 ||
           statusCode == 307 ||
           statusCode == 308;
}

bool StatusCode::isError(int statusCode)
{
    return statusCode >= 400 && statusCode <= 599;
}