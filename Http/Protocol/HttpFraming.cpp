#include "HttpFraming.hpp"
#include "../../utils/StringUtils.hpp"
#include <sstream>

size_t HttpFraming::decimalSize(const std::string &value)
{
    size_t result;
    size_t i;

    result = 0;
    i = 0;
    while (i < value.length())
    {
        if (value[i] < '0' || value[i] > '9')
            return 0;
        result = result * 10 + static_cast<size_t>(value[i] - '0');
        i++;
    }
    return result;
}

bool HttpFraming::findHeaderEnd(const std::vector<char> &buffer,
                                size_t &headerEnd)
{
    size_t i;

    if (buffer.size() < 4)
        return false;
    i = 0;
    while (i + 3 < buffer.size())
    {
        if (buffer[i] == '\r'
            && buffer[i + 1] == '\n'
            && buffer[i + 2] == '\r'
            && buffer[i + 3] == '\n')
        {
            headerEnd = i;
            return true;
        }
        i++;
    }
    return false;
}

std::string HttpFraming::_getHeaderValue(const std::string &headerPart,
                                         const std::string &key)
{
    std::istringstream stream(headerPart);
    std::string line;
    std::string lowerKey;
    size_t colonPos;
    std::string headerName;
    std::string headerValue;

    lowerKey = StringUtils::toLowerAscii(key);
    while (std::getline(stream, line))
    {
        if (!line.empty() && line[line.length() - 1] == '\r')
            line.erase(line.length() - 1);

        colonPos = line.find(':');
        if (colonPos == std::string::npos)
            continue;

        headerName = StringUtils::toLowerAscii(line.substr(0, colonPos));
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

bool HttpFraming::isRequestComplete(const std::vector<char> &buffer)
{
    size_t headerEnd;
    std::string headerPart;
    std::string contentLength;
    std::string transferEncoding;
    size_t expectedBodySize;
    size_t bodyStart;

    if (!findHeaderEnd(buffer, headerEnd))
        return false;

    headerPart.assign(buffer.begin(), buffer.begin() + headerEnd);
    bodyStart = headerEnd + 4;

    transferEncoding = StringUtils::toLowerAscii(
        _getHeaderValue(headerPart, "Transfer-Encoding"));
    if (transferEncoding.find("chunked") != std::string::npos)
    {
        size_t i;

        i = bodyStart;
        while (i + 6 < buffer.size())
        {
            if (buffer[i] == '\r'
                && buffer[i + 1] == '\n'
                && buffer[i + 2] == '0'
                && buffer[i + 3] == '\r'
                && buffer[i + 4] == '\n'
                && buffer[i + 5] == '\r'
                && buffer[i + 6] == '\n')
                return true;
            i++;
        }
        if (bodyStart + 4 < buffer.size()
            && buffer[bodyStart] == '0'
            && buffer[bodyStart + 1] == '\r'
            && buffer[bodyStart + 2] == '\n'
            && buffer[bodyStart + 3] == '\r'
            && buffer[bodyStart + 4] == '\n')
            return true;
        return false;
    }

    contentLength = _getHeaderValue(headerPart, "Content-Length");
    if (!contentLength.empty())
    {
        expectedBodySize = decimalSize(contentLength);
        return buffer.size() - bodyStart >= expectedBodySize;
    }
    return true;
}

void HttpFraming::reserveForContentLength(std::vector<char> &buffer)
{
    size_t headerEnd;
    std::string headerPart;
    std::string contentLength;
    size_t expectedBodySize;
    size_t expectedTotalSize;

    if (!findHeaderEnd(buffer, headerEnd))
        return;

    headerPart.assign(buffer.begin(), buffer.begin() + headerEnd);
    contentLength = _getHeaderValue(headerPart, "Content-Length");
    if (contentLength.empty())
        return;

    expectedBodySize = decimalSize(contentLength);
    if (expectedBodySize > 1024 * 1024)
        return;
    expectedTotalSize = headerEnd + 4 + expectedBodySize;
    if (buffer.capacity() < expectedTotalSize)
        buffer.reserve(expectedTotalSize);
}
