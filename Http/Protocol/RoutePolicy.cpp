#include "RoutePolicy.hpp"

bool RoutePolicy::isMethodAllowed(const RouteResult &route,
                                  const std::string &method)
{
    size_t i;

    if (route.location == NULL)
        return false;
    if (!route.location->hasAllowedMethods)
        return method == "GET";

    i = 0;
    while (i < route.location->allowedMethods.size())
    {
        if (route.location->allowedMethods[i] == method)
            return true;
        i++;
    }
    return false;
}

std::string RoutePolicy::allowedMethods(const RouteResult &route)
{
    std::string allowed;
    size_t i;

    if (route.location == NULL || !route.location->hasAllowedMethods)
        return "GET";

    i = 0;
    while (i < route.location->allowedMethods.size())
    {
        if (!allowed.empty())
            allowed += ", ";
        allowed += route.location->allowedMethods[i];
        i++;
    }
    if (allowed.empty())
        return "GET";
    return allowed;
}
