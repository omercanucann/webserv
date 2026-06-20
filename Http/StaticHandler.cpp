#include "StaticHandler.hpp"

StaticHandler::StaticHandler() : _autoIndex()
{

}

bool StaticHandler::_isUploadEnabled(const RouteResult &route) const
{
    if (route.location == NULL)
        return false;
    
    if (!route.location->hasUploadEnabled)
        return false;
    
    return route.location->uploadEnabled;
}

bool StaticHandler::_isBodyTooLarge(const HttpRequest &request, const RouteResult &route) const
{
    if (route.server && route.server->hasClientMaxBodySize)
        if (request.getBody().size() > route.server->clientMaxBodySize)
            return true;
    
    return false;
}

bool StaticHandler::_ensureDirectory(const std::string &path) const
{
    struct stat st;

    if (stat(path.c_str(), &st) == 0)
        return S_ISDIR(st.st_mode);

    if (mkdir(path.c_str(), 0755) != 0)
        return false;

    return true;
}

bool StaticHandler::_writeFile(const std::string &path, const std::string &content) const
{
    std::ofstream file;

    file.open(path.c_str(), std::ios::out | std::ios::binary);
    if (!file.is_open())
        return false;
    
    file.write(content.c_str(), content.size());

    if (!file.good())
    {
        file.close();
        return false;
    }
    file.close();
    return true;
}

std::string StaticHandler::_getUploadPath(const RouteResult &route) const
{
    if (route.location && route.location->hasUploadPath)
        return route.location->uploadPath;

    return "uploads";
}

std::string StaticHandler::_generateUploadFileName() const
{
    static unsigned long counter;
    std::ostringstream oss;

    oss << "upload_";
    oss << std::time(NULL);
    oss << "_";
    oss << counter++;
    oss << ".bin";

    return oss.str();
}


HttpResponse StaticHandler::_handlePost(const HttpRequest &request, const RouteResult &route)
{
    HttpResponse response;
    std::string uploadDir;
    std::string fileName;
    std::string filePath;
    std::string html;
    std::string fileContent;
    std::string contentType;
    std::string publicUrl;

    if (!_isUploadEnabled(route))
        return HttpResponse::makeErrorResponse(403);

    if (_isBodyTooLarge(request, route))
        return HttpResponse::makeErrorResponse(413);

    uploadDir = _getUploadPath(route);

    if (!_ensureDirectory(uploadDir))
        return HttpResponse::makeErrorResponse(500);

    fileContent = request.getBody();
    fileName = _generateUploadFileName();

    contentType = request.getHeader("Content-Type");

    if (contentType.find("multipart/form-data") != std::string::npos)
    {
        size_t boundaryPos;
        std::string boundary;
        size_t partStart;
        size_t headerStart;
        size_t headerEnd;
        size_t dataStart;
        size_t dataEnd;
        std::string partHeaders;
        size_t namePos;
        size_t nameEnd;

        boundaryPos = contentType.find("boundary=");
        if (boundaryPos == std::string::npos)
            return HttpResponse::makeErrorResponse(400);

        boundary = "--" + contentType.substr(boundaryPos + 9);

        partStart = fileContent.find(boundary);
        if (partStart == std::string::npos)
            return HttpResponse::makeErrorResponse(400);

        headerStart = fileContent.find("\r\n", partStart);
        if (headerStart == std::string::npos)
            return HttpResponse::makeErrorResponse(400);
        headerStart += 2;

        headerEnd = fileContent.find("\r\n\r\n", headerStart);
        if (headerEnd == std::string::npos)
            return HttpResponse::makeErrorResponse(400);

        partHeaders = fileContent.substr(headerStart, headerEnd - headerStart);

        dataStart = headerEnd + 4;
        dataEnd = fileContent.find("\r\n" + boundary, dataStart);
        if (dataEnd == std::string::npos)
            return HttpResponse::makeErrorResponse(400);

        namePos = partHeaders.find("filename=\"");
        if (namePos != std::string::npos)
        {
            namePos += 10;
            nameEnd = partHeaders.find("\"", namePos);
            if (nameEnd != std::string::npos)
            {
                std::string originalName;
                size_t slashPos;
                size_t i;

                originalName = partHeaders.substr(namePos, nameEnd - namePos);
                slashPos = originalName.find_last_of("/\\");
                if (slashPos != std::string::npos)
                    originalName = originalName.substr(slashPos + 1);

                i = 0;
                while (i < originalName.length())
                {
                    char c = originalName[i];

                    if (!((c >= 'a' && c <= 'z')
                        || (c >= 'A' && c <= 'Z')
                        || (c >= '0' && c <= '9')
                        || c == '.'
                        || c == '_'
                        || c == '-'))
                        originalName[i] = '_';
                    i++;
                }

                if (!originalName.empty())
                {
                    std::ostringstream oss;

                    oss << "upload_";
                    oss << std::time(NULL);
                    oss << "_";
                    oss << originalName;

                    fileName = oss.str();
                }
            }
        }

        fileContent = fileContent.substr(dataStart, dataEnd - dataStart);
    }

    filePath = _joinPath(uploadDir, fileName);

    if (!_writeFile(filePath, fileContent))
        return HttpResponse::makeErrorResponse(500);

    publicUrl = "/uploads/" + fileName;

    html = "<html><body>";
    html += "<h1>Upload successful</h1>";
    html += "<p>Saved as: ";
    html += fileName;
    html += "</p>";

    html += "<p><a href=\"";
    html += publicUrl;
    html += "\">Open uploaded file</a></p>";

    html += "<img src=\"";
    html += publicUrl;
    html += "\" style=\"max-width:600px;display:block;margin-top:10px;\">";

    html += "</body></html>\n";

    response.setStatus(201);
    response.setHeader("Content-Type", "text/html");
    response.setBody(html);

    return response;
}



bool StaticHandler::_isAutoIndexEnabled(const RouteResult &route) const
{
    if (route.location && route.location->hasAutoIndex)
        return route.location->autoindex;

    return false;
}

std::string StaticHandler::_joinPath(const std::string &root, const std::string &path) const
{
    if (root.empty())
        return path;

    if (path.empty())
        return root;

    if (root[root.length() - 1] == '/' && path[0] == '/')
        return root + path.substr(1);
    
    if (root[root.length() - 1] != '/' && path[0] != '/')
        return root + "/" + path;

    return root + path;
}

std::string StaticHandler::_buildFilePath(const HttpRequest &request, const RouteResult &route) const
{
    std::string root;
    std::string path;
    std::string locationPath;

    root = _getRoot(route);
    path = request.getPath();

    if (route.location)
        locationPath = route.location->path;

    if (locationPath != "/" && path.compare(0, locationPath.length(), locationPath) == 0)
        path = path.substr(locationPath.length());

    return _joinPath(root, path);
}

std::string StaticHandler::_resolveGetPath(const HttpRequest &request, const RouteResult &route) const
{
    std::string filePath;
    std::string indexPath;

    filePath = _buildFilePath(request, route);

    if (_isDirectory(filePath))
    {
        indexPath = _joinPath(filePath, _getIndex(route));

        if (_fileExists(indexPath) && !_isDirectory(indexPath))
            return indexPath;
    }
    return filePath;
}

bool StaticHandler::_containsPathTraversal(const std::string &path) const
{
    if (path.find("..") != std::string::npos)
        return true;
    
    return false;
}

bool StaticHandler::_isDirectory(const std::string &path) const
{
    struct stat st;

    if (stat(path.c_str(), &st) != 0)
        return false;

    return S_ISDIR(st.st_mode);
}

bool StaticHandler::_fileExists(const std::string &path) const
{
    struct stat st;

    if (stat(path.c_str(), &st) != 0)
        return false;

    return true;
}

bool StaticHandler::_readFile(const std::string &path, std::string &out) const
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

HttpResponse StaticHandler::_handleGet(const HttpRequest &request, const RouteResult &route)
{
    HttpResponse response;
    std::string filePath;
    std::string body;

    if (_containsPathTraversal(request.getPath()))
        return HttpResponse::makeErrorResponse(403);
    
    filePath = _resolveGetPath(request, route);

    if (!_fileExists(filePath))
        return HttpResponse::makeErrorResponse(404);

    if (_isDirectory(filePath))
    {
        if (_isAutoIndexEnabled(route))
            return _autoIndex.generate(filePath, request.getPath());
    
        return HttpResponse::makeErrorResponse(403);
    }

    if (!_readFile(filePath, body))
        return HttpResponse::makeErrorResponse(403);

    response.setStatus(200);
    response.setHeader("Content-Type", _mimeTypes.getMimeType(filePath));
    response.setBody(body);
    return response;
}

HttpResponse StaticHandler::_handleDelete(const HttpRequest &request, const RouteResult &route)
{
    HttpResponse response;
    std::string filePath;

    if (_containsPathTraversal(request.getPath()))
        return HttpResponse::makeErrorResponse(403);

    filePath = _buildFilePath(request, route);

    if (!_fileExists(filePath))
        return HttpResponse::makeErrorResponse(404);

    if (_isDirectory(filePath))
        return HttpResponse::makeErrorResponse(403);

    if (std::remove(filePath.c_str()) != 0)
        return HttpResponse::makeErrorResponse(403);

    response.setStatus(204);
    return response;
}

std::string StaticHandler::_getRoot(const RouteResult &route) const
{
    if (route.location && route.location->hasRoot)
        return route.location->root;
    
    if (route.server && route.server->hasRoot)
        return route.server->root;

    return "www";
}

std::string StaticHandler::_getIndex(const RouteResult &route) const
{
    if (route.location && route.location->hasIndex)
        return route.location->index;

    if (route.server && route.server->hasIndex)
        return route.server->index;

    return "index.html";
}

std::string StaticHandler::_allowedMethodsToString(const RouteResult &route) const
{
    std::string allowedStr;
    size_t i = 0;

    if (route.location == NULL || !route.location->hasAllowedMethods)
        return "GET";

    while (i < route.location->allowedMethods.size())
    {
        if (!allowedStr.empty())
            allowedStr += ", ";
        allowedStr += route.location->allowedMethods[i];
        i++;
    }
    if (allowedStr.empty())
        return "GET";

    return allowedStr;
}

bool StaticHandler::_isMethodAllowed(const std::string &method, const RouteResult &route) const
{
    size_t i = 0;

    if (!route.location->hasAllowedMethods)
        return method == "GET";
    
    while (i < route.location->allowedMethods.size())
    {
        if (route.location->allowedMethods[i] == method)
            return true;
        i++;
    }
    return false;
}

HttpResponse StaticHandler::handle(const HttpRequest &request, const RouteResult &route)
{
    if (route.server == NULL || route.location == NULL)
        return HttpResponse::makeErrorResponse(404);
    
    if (route.location->hasRedirect)
        return HttpResponse::makeRedirectResponse(route.location->redirectStatus, route.location->redirectTarget);
    
    if (!_isMethodAllowed(request.getMethod(), route))
        return HttpResponse::makeMethodNotAllowedResponse(_allowedMethodsToString(route));

    if (request.getMethod() == "GET")
        return _handleGet(request, route);
    
    if (request.getMethod() == "DELETE")
        return _handleDelete(request, route);
    
    if (request.getMethod() == "POST")
        return _handlePost(request, route);

    return HttpResponse::makeErrorResponse(405);
}
