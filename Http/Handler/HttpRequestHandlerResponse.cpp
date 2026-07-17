#include "HttpRequestHandler.hpp"
#include "../../utils/FileUtils.hpp"

HttpResponse HttpRequestHandler::_makeErrorResponse(
    int statusCode,
    const ServerConfig *server) const
{
    HttpResponse response;
    std::string filePath;
    std::string body;
    std::map<int, std::string>::const_iterator it;

    if (server != NULL)
    {
        it = server->errorPages.find(statusCode);
        if (it != server->errorPages.end())
        {
            filePath = it->second;
            if (!filePath.empty() && filePath[0] == '/' && server->hasRoot)
                filePath = FileUtils::joinPath(server->root, filePath);

            if (FileUtils::exists(filePath)
                && !FileUtils::isDirectory(filePath)
                && FileUtils::readFile(filePath, body))
            {
                response.setStatus(statusCode);
                response.setHeader("Content-Type", "text/html");
                response.setBody(body);
                return response;
            }
        }
    }
    return HttpResponse::makeErrorResponse(statusCode);
}

bool HttpRequestHandler::_dispatchRequest(int slot_index,
                                          const HttpRequest &request,
                                          HttpResponse &response)
{
    RouteResult route;

    route = _router.route(request);
    if (route.server == NULL)
    {
        response = _makeErrorResponse(500, NULL);
        return true;
    }
    if (route.location == NULL)
    {
        response = _makeErrorResponse(404, route.server);
        return true;
    }

    if (_cgiHandler.isCgiRequest(request, route))
    {
        if (_cgiHandler.start(slot_index, request, route))
            return false;
        response = _makeErrorResponse(500, route.server);
        return true;
    }

    if (request.getMethod() == "GET"
        || request.getMethod() == "DELETE"
        || request.getMethod() == "POST")
    {
        response = _staticHandler.handle(request, route);
        if (response.getStatusCode() >= 400
            && response.getStatusCode() <= 599)
            response = _makeErrorResponse(response.getStatusCode(), route.server);
        return true;
    }

    response = _makeErrorResponse(405, route.server);
    return true;
}

void HttpRequestHandler::_storeFinalResponse(ConnectionSlot &slot,
                                             HttpResponse &response) const
{
    std::string rawResponse;

    rawResponse = response.toString();
    slot.writebuffer.assign(rawResponse.begin(), rawResponse.end());
    slot.write_position = 0;
    slot.request_complete = true;
    slot.state = ConnectState_WRITING;
}
