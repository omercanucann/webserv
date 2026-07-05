#ifndef HTTPREQUESTHANDLER_HPP
#define HTTPREQUESTHANDLER_HPP

#include "Reactorbridge.hpp"
#include "HttpParser.hpp"
#include "HttpRequest.hpp"
#include "HttpResponse.hpp"
#include <string>
#include <vector>
#include "../Router/Router.hpp"
#include "StaticHandler.hpp"
#include "CgiHandler.hpp"

class HttpRequestHandler : public IRequestHandler
{
    public:
        HttpRequestHandler(const Config &config, PollReactor &reactor);
        virtual ~HttpRequestHandler();

        virtual bool handle_data(int slot_index, ConnectionSlot &slot);
        virtual void handle_close(int slot_index);

	private:
		HttpParser _parser;
		Router     _router;
		StaticHandler _staticHandler;
        CgiHandler _cgiHandler;

        bool        _isRequestComplete(const std::string &rawRequest) const;
        bool        _isRequestComplete(const std::vector<char> &rawRequest) const;
        bool        _hasHeaderEnd(const std::string &rawRequest, size_t &headerEnd) const;
        bool        _hasHeaderEnd(const std::vector<char> &rawRequest, size_t &headerEnd) const;
        std::string _getHeaderValue(const std::string &headerPart, const std::string &key) const;
        std::string _toLower(const std::string &str) const;
        size_t      _stringToSize(const std::string &str) const;
        bool        _parseHeaderOnlyRequest(const std::string &rawRequest, HttpRequest &request) const;
        bool        _isMethodAllowedByRoute(const std::string &method, const RouteResult &route) const;
        std::string _allowedMethodsToString(const RouteResult &route) const;
        bool        _makeEarlyResponse(const std::string &rawRequest, HttpResponse &response) const;
        bool        _makeEarlyResponse(const std::vector<char> &rawRequest, HttpResponse &response) const;
        bool        _handleExpectContinueCgi(int slot_index, const std::vector<char> &rawRequest);
        void        _reserveRequestBuffer(ConnectionSlot &slot) const;
        void        _sendContinueIfNeeded(ConnectionSlot &slot) const;
        static size_t _getParserLimit(const Config &config);

		bool _buildResponse(int slot_index, const HttpRequest &request, HttpResponse &response);

		bool        _isDirectory(const std::string &path) const;
		bool        _fileExists(const std::string &path) const;
		bool        _readFile(const std::string &path, std::string &out) const;
        HttpResponse _makeErrorResponse(int statusCode, const ServerConfig *server) const;
};

#endif
