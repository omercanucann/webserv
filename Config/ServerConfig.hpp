#ifndef SERVER_CONFIG_HPP
# define SERVER_CONFIG_HPP

#include <string>
#include <vector>
#include <map>
#include "LocationConfig.hpp"
#include <cstdlib>


struct ServerConfig
{
    std::string listenHost;
    int listenPort;
    bool hasListen;

    std::string root;
    bool hasRoot;

    std::string index;
    bool hasIndex;

    size_t clientMaxBodySize;
    bool hasClientMaxBodySize;

    std::map<int, std::string> errorPages;

    std::vector<LocationConfig> locations;

    ServerConfig() :
        listenHost("0.0.0.0"),
        listenPort(0),
        hasListen(false),
        hasRoot(false),
        hasIndex(false),
        clientMaxBodySize(0),
        hasClientMaxBodySize(false)
        {}
};


#endif