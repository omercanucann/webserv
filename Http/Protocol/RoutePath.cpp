#include "RoutePath.hpp"
#include "../../utils/FileUtils.hpp"

std::string RoutePath::_root(const RouteResult &route)
{
    if (route.location != NULL && route.location->hasRoot)
        return route.location->root;
    if (route.server != NULL && route.server->hasRoot)
        return route.server->root;
    return "www";
}

std::string RoutePath::resolve(const HttpRequest &request,
                               const RouteResult &route)
{
    std::string path;
    std::string locationPath;

    path = request.getPath();
    if (route.location != NULL)
        locationPath = route.location->path;

    if (locationPath != "/"
        && path.compare(0, locationPath.length(), locationPath) == 0)
        path = path.substr(locationPath.length());

    return FileUtils::joinPath(_root(route), path);
}
