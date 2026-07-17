#include "HttpRequestHandler.hpp"
#include <fstream>
#include <sstream>
#include <sys/stat.h>

HttpResponse HttpRequestHandler::_makeErrorResponse(
    int statusCode,
    const ServerConfig *server) const
{
    HttpResponse response;
    std::string filePath;
    std::string body;
    std::map<int, std::string>::const_iterator it;
    struct stat fileStat;

    if (server != NULL)
    {
        it = server->errorPages.find(statusCode);
        if (it != server->errorPages.end())
        {
            filePath = it->second;
            if (!filePath.empty() && filePath[0] == '/' && server->hasRoot)
            {
                if (!server->root.empty()
                    && server->root[server->root.length() - 1] == '/')
                    filePath = server->root + filePath.substr(1);
                else
                    filePath = server->root + filePath;
            }

            if (stat(filePath.c_str(), &fileStat) == 0
                && !S_ISDIR(fileStat.st_mode)
                && _readFile(filePath, body))
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

bool HttpRequestHandler::_readFile(const std::string &path,
                                   std::string &out) const
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
