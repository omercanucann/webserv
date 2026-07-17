#ifndef ROUTEPATH_HPP
#define ROUTEPATH_HPP

#include "HttpRequest.hpp"
#include "../../Router/Router.hpp"
#include <string>

class RoutePath
{
    private:
        static std::string _root(const RouteResult &route);

    public:
        static std::string resolve(const HttpRequest &request,
                                   const RouteResult &route);
};

#endif
