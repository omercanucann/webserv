#ifndef CONFIG_HPP
# define CONFIG_HPP

#include <vector>
#include "ServerConfig.hpp"

struct Config
{
    std::string configName;
    std::string configPath;
    std::vector<ServerConfig> servers;
};

#endif