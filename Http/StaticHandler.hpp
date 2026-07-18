#ifndef STATICHANDLER
# define STATICHANDLER

#include "AutoIndex.hpp"
#include "HttpResponse.hpp"
#include "HttpRequest.hpp"
#include "MimeTypes.hpp"
#include "../Router/Router.hpp"
#include <sys/stat.h>
#include <fstream>
#include <sstream>
#include <cstdio>
#include <ctime>


class StaticHandler
{
	private:
		MimeTypes _mimeTypes;
		AutoIndex _autoIndex;

		bool _isAutoIndexEnabled(const RouteResult &route) const;

		HttpResponse _handleGet(const HttpRequest &request, const RouteResult &route);
		HttpResponse _handleDelete(const HttpRequest &request, const RouteResult &route);
	
		bool _isMethodAllowed(const std::string &method, const RouteResult &route) const;
		std::string _allowedMethodsToString(const RouteResult &route) const;


		std::string _getRoot(const RouteResult &route) const;
        std::string _getIndex(const RouteResult &route) const;
        std::string _joinPath(const std::string &root, const std::string &path) const;
        std::string _buildFilePath(const HttpRequest &request, const RouteResult &route) const;
        std::string _resolveGetPath(const HttpRequest &request, const RouteResult &route) const;

        bool _containsPathTraversal(const std::string &path) const;
        bool _isDirectory(const std::string &path) const;
        bool _fileExists(const std::string &path) const;
        bool _readFile(const std::string &path, std::string &out) const;


		HttpResponse _handlePost(const HttpRequest &request, const RouteResult &route);

		bool _isUploadEnabled(const RouteResult &route) const;
		bool _isBodyTooLarge(const HttpRequest &request, const RouteResult &route) const;
		bool _directoryExists(const std::string &path) const;
		bool _writeFile(const std::string &path, const std::string &content) const;
		std::string _getUploadPath(const RouteResult &route) const;
		std::string _generateUploadFileName() const;

		public:
		StaticHandler();
		HttpResponse handle(const HttpRequest &request, const RouteResult &route);
};

#endif
