#ifndef STATICHANDLER
# define STATICHANDLER

#include "AutoIndex.hpp"
#include "HttpResponse.hpp"
#include "HttpRequest.hpp"
#include "MimeTypes.hpp"
#include "../../Router/Router.hpp"


class StaticHandler
{
	private:
		MimeTypes _mimeTypes;
		AutoIndex _autoIndex;

		bool _isAutoIndexEnabled(const RouteResult &route) const;

		HttpResponse _handleGet(const HttpRequest &request, const RouteResult &route);
		HttpResponse _handleDelete(const HttpRequest &request, const RouteResult &route);
	
        std::string _getIndex(const RouteResult &route) const;
        std::string _resolveGetPath(const HttpRequest &request, const RouteResult &route) const;

        bool _containsPathTraversal(const std::string &path) const;
		HttpResponse _handlePost(const HttpRequest &request, const RouteResult &route);

		bool _isUploadEnabled(const RouteResult &route) const;
		bool _isBodyTooLarge(const HttpRequest &request, const RouteResult &route) const;
		bool _ensureDirectory(const std::string &path) const;
		bool _writeFile(const std::string &path, const std::string &content) const;
		std::string _getUploadPath(const RouteResult &route) const;
		std::string _generateUploadFileName() const;

		public:
		HttpResponse handle(const HttpRequest &request, const RouteResult &route);
};

#endif
