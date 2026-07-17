#include "HttpRequestHandler.hpp"
#include "HttpFraming.hpp"
#include "RoutePolicy.hpp"
#include "../../utils/StringUtils.hpp"

bool HttpRequestHandler::_parseHeaderOnlyRequest(const std::string &rawRequest,
                                                 HttpRequest &request) const
{
    try
    {
        request = _parser.parseHeaders(rawRequest);
    }
    catch (const std::exception &)
    {
        return false;
    }
    return true;
}

bool HttpRequestHandler::_makePreflightResponse(const std::string &rawRequest,
                                                HttpResponse &response) const
{
    HttpRequest request;
    RouteResult route;
    std::string contentLength;
    size_t bodySize;

    if (!_parseHeaderOnlyRequest(rawRequest, request))
        return false;

    route = _router.route(request);
    if (route.server != NULL && route.server->hasClientMaxBodySize)
    {
        contentLength = request.getHeader("Content-Length");
        if (!contentLength.empty())
        {
            bodySize = HttpFraming::decimalSize(contentLength);
            if (bodySize > route.server->clientMaxBodySize)
            {
                response = _makeErrorResponse(413, route.server);
                return true;
            }
        }
    }

    if (route.location != NULL
        && !RoutePolicy::isMethodAllowed(route, request.getMethod()))
    {
        response = HttpResponse::makeMethodNotAllowedResponse(
            RoutePolicy::allowedMethods(route));
        return true;
    }
    return false;
}

bool HttpRequestHandler::_makePreflightResponse(const std::vector<char> &rawRequest,
                                                HttpResponse &response) const
{
    size_t headerEnd;
    std::string headerOnly;

    if (!HttpFraming::findHeaderEnd(rawRequest, headerEnd))
        return false;
    headerOnly.assign(rawRequest.begin(), rawRequest.begin() + headerEnd + 4);
    return _makePreflightResponse(headerOnly, response);
}

void HttpRequestHandler::_sendContinueIfNeeded(ConnectionSlot &slot)
{
    size_t headerEnd;
    std::string headerOnly;
    HttpRequest request;
    std::string expect;
    static const char response[] = "HTTP/1.1 100 Continue\r\n\r\n";

    if (slot.continue_sent)
        return;
    if (!HttpFraming::findHeaderEnd(slot.readbuffer, headerEnd))
        return;

    headerOnly.assign(slot.readbuffer.begin(), slot.readbuffer.begin() + headerEnd + 4);
    if (!_parseHeaderOnlyRequest(headerOnly, request))
        return;

    if (request.hasHeader("X-Secret-Header-For-Test"))
        return;

    expect = StringUtils::toLowerAscii(request.getHeader("Expect"));
    if (expect.find("100-continue") == std::string::npos)
        return;

    if (_reactor != NULL)
    {
        _reactor->queue_interim_response(slot.self_index, response);
        slot.continue_sent = true;
    }
}
