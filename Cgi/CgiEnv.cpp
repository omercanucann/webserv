#include "CgiEnv.hpp"

CgiEnv::CgiEnv()
{

}

std::string CgiEnv::_toString(size_t value) const
{
    std::ostringstream oss;
    oss << value;
    return oss.str();
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

    _add("REQUEST_METHOD", request.getMethod());
    _add("QUERY_STRING", request.getQuery());
    _add("SCRIPT_FILENAME", scriptPath);
    _add("SCRIPT_NAME", request.getPath());
    _add("PATH_INFO", pathInfo);
    _add("SERVER_NAME", "localhost");

    if (route.server)
        _add("SERVER_PORT", _toString(route.server->listenPort));
    if (request.hasHeader("Content-Type"))
        _add("CONTENT_TYPE", request.getHeader("Content-Type"));
    if (request.getMethod() == "POST")
        _add("CONTENT_LENGTH", _toString(request.getBody().size()));
    
    while (i < _envStrings.size())
    {
        _envp.push_back(const_cast<char *>(_envStrings[i].c_str()));
        i++;
    }

    _envp.push_back(NULL);
    return &_envp[0];
}
