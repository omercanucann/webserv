#include "Router.hpp"


Router::Router(const Config &config) : _config(config)
{

}

std::string Router::_getHostName(const std::string &hostHeader) const
{
    size_t pos = hostHeader.find(":");
    if (pos == std::string::npos)
        return hostHeader;
    
    return hostHeader.substr(0, pos);
}

int	Router::_getHostPort(const std::string &hostHeader) const
{
     size_t pos;

    pos = hostHeader.find(":");
    if (pos == std::string::npos)
        return 80;

    return std::atoi(hostHeader.substr(pos + 1).c_str());
}

const ServerConfig *Router::selectServer(const HttpRequest &request) const
{
    std::string hostHeader;
    std::string hostName;
    int hostPort;
    size_t i = 0;

    if (_config.servers.empty())
        return NULL;
    
    hostHeader = request.getHeader("Host");
    hostName = _getHostName(hostHeader);
    hostPort = _getHostPort(hostHeader);

    while (i < _config.servers.size())
    {
        if (_config.servers[i].listenPort == hostPort)
        {
            if (_config.servers[i].listenHost == "0.0.0.0"
                || _config.servers[i].listenHost == hostName
                || (hostName == "localhost" && _config.servers[i].listenHost == "127.0.0.1"))
                return &_config.servers[i];
        }
        i++;
    }
    return &_config.servers[0];
}

bool Router::_locationMatches(const std::string &requestPath, const std::string &locationPath) const
{
    if (locationPath == "/")
        return true;
    if (requestPath.substr(0, locationPath.length()) != locationPath)
        return false;
    if (requestPath.length() == locationPath.length())
        return true;
    
    return requestPath[locationPath.length()] == '/';
}

const LocationConfig *Router::matchLocation(const ServerConfig &server, const std::string &path) const
{
    const LocationConfig *best = NULL;
    size_t bestLen = 0;
    size_t i = 0;

    while (i < server.locations.size())
    {
        if (_locationMatches(path, server.locations[i].path))
        {
            if(server.locations[i].path.length() > bestLen)
            {
                best = &server.locations[i];
                bestLen = server.locations[i].path.length();
            }
        }
        i++;
    }
    return best;
}


RouteResult Router::route(const HttpRequest &request) const
{
    RouteResult result;

    result.server = selectServer(request);
    if (result.server)
        result.location = matchLocation(*result.server, request.getPath());

    return result;
}