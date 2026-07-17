#ifndef HTTPREQUESTHANDLER_HPP
#define HTTPREQUESTHANDLER_HPP

#include "Reactorbridge.hpp"
#include "HttpParser.hpp"
#include "HttpRequest.hpp"
#include "HttpResponse.hpp"
#include <string>
#include <vector>
#include "../../Router/Router.hpp"
#include "StaticHandler.hpp"
#include "CgiHandler.hpp"
#include <map>

class HttpRequestHandler : public IRequestHandler
{
    public:
        HttpRequestHandler(const Config &config, PollReactor &reactor);
        virtual ~HttpRequestHandler();

        virtual bool handle_data(int slot_index, ConnectionSlot &slot);
        virtual void handle_close(int slot_index);
        virtual bool handle_timeout(int slot_index, ConnectionSlot &slot);

    private:
        HttpParser _parser;
        Router _router;
        StaticHandler _staticHandler;
        CgiHandler _cgiHandler;
        PollReactor *_reactor;

        // Per-connection state used only while a CGI request body is streaming.
        struct BodyStreamState;
        std::map<int, BodyStreamState *> _bodyStreams;

        // Header-only checks that may reject a request before its body arrives.
        bool        _parseHeaderOnlyRequest(const std::string &rawRequest, HttpRequest &request) const;
        bool        _makePreflightResponse(const std::string &rawRequest, HttpResponse &response) const;
        bool        _makePreflightResponse(const std::vector<char> &rawRequest, HttpResponse &response) const;
        void        _sendContinueIfNeeded(ConnectionSlot &slot);

        // Response creation and synchronous/asynchronous dispatch.
        HttpResponse _makeErrorResponse(int statusCode, const ServerConfig *server) const;
        bool         _readFile(const std::string &path, std::string &out) const;
        bool         _dispatchRequest(int slot_index, const HttpRequest &request, HttpResponse &response);
        void         _storeFinalResponse(ConnectionSlot &slot, HttpResponse &response) const;

        // CGI request-body streaming. Implemented in HttpRequestHandlerCgi.cpp.
        bool        _handleCgiBodyStream(int slot_index, ConnectionSlot &slot, bool &handled);
        bool        _startCgiBodyStream(int slot_index, ConnectionSlot &slot, bool &handled);
        bool        _processCgiBodyStream(int slot_index, ConnectionSlot &slot);
        bool        _finishCgiBodyStream(int slot_index, ConnectionSlot &slot);
        bool        _failCgiBodyStream(int slot_index, ConnectionSlot &slot, int statusCode);
        void        _cleanupBodyStream(int slot_index);
        bool        _parseSizeValue(const std::string &value, size_t &out) const;
        bool        _parseChunkSizeLine(const std::string &line, size_t &out) const;
        bool        _bodyLimitForRoute(const RouteResult &route, size_t &limit) const;

        static size_t _getParserLimit(const Config &config);
};

#endif
