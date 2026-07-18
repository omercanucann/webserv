#include "CgiEnv.hpp"
#include "../utils/StringUtils.hpp"

CgiEnv::CgiEnv()
{

}

void CgiEnv::_add(const std::string &key, const std::string &value)
{
    _envStrings.push_back(key + "=" + value);
}

char **CgiEnv::build(const HttpRequest &request, const RouteResult &route, const std::string &scriptPath, const std::string &pathInfo)
{
    _envStrings.clear();
    _envp.clear();
    size_t i = 0;
    std::string requestUri;

    requestUri = request.getPath();
    if (!request.getQuery().empty())
        requestUri += "?" + request.getQuery();
    _add("REQUEST_METHOD", request.getMethod());
    _add("SERVER_PROTOCOL", request.getVersion());
    _add("REQUEST_URI", requestUri);
    _add("QUERY_STRING", request.getQuery());
    _add("SCRIPT_FILENAME", scriptPath);
    _add("SCRIPT_NAME", request.getPath());
    _add("PATH_INFO", pathInfo);
    _add("SERVER_NAME", "localhost");

    if (route.server)
        _add("SERVER_PORT", StringUtils::sizeToString(route.server->listenPort));
    if (request.hasHeader("Content-Type"))
        _add("CONTENT_TYPE", request.getHeader("Content-Type"));
    if (request.getMethod() == "POST")
        _add("CONTENT_LENGTH", StringUtils::sizeToString(request.getBodySize()));
    if (request.hasHeader("X-Secret-Header-For-Test"))
        _add("HTTP_X_SECRET_HEADER_FOR_TEST", request.getHeader("X-Secret-Header-For-Test"));
    
    while (i < _envStrings.size())
    {
        _envp.push_back(const_cast<char *>(_envStrings[i].c_str()));
        i++;
    }

    _envp.push_back(NULL);
    return &_envp[0];
}
