#ifndef ROUTEPOLICY_HPP
#define ROUTEPOLICY_HPP

#include "../../Router/Router.hpp"
#include <string>

class RoutePolicy
{
    public:
        static bool isMethodAllowed(const RouteResult &route,
                                    const std::string &method);
        static std::string allowedMethods(const RouteResult &route);
};

#endif
