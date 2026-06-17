#ifndef ROUTER_HPP
# define ROUTER_HPP

#include "../Config/Config.hpp"
#include "../Http/HttpRequest.hpp"
#include <string>
#include <cstdlib>

struct RouteResult
{
	const ServerConfig *server;
	const LocationConfig *location;

	RouteResult() : server(NULL), location(NULL) {}
};

class Router
{
	private:
		const Config &_config;

		bool _locationMatches(const std::string &requestPath, const std::string &locationPath) const;
		
		std::string _getHostName(const std::string &hostHeader) const;
		int	_getHostPort(const std::string &hostHeader) const;
	public:
		Router(const Config &config);

		RouteResult route(const HttpRequest &request) const;
		const ServerConfig *selectServer(const HttpRequest &request) const;
		const LocationConfig *matchLocation(const ServerConfig &server, const std::string &path) const;
};

#endif