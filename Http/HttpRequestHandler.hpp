#ifndef HTTPREQUESTHANDLER_HPP
#define HTTPREQUESTHANDLER_HPP

#include "Reactorbridge.hpp"
#include "HttpParser.hpp"
#include "HttpRequest.hpp"
#include "HttpResponse.hpp"
#include "MimeTypes.hpp"
#include <string>
#include <vector>

class HttpRequestHandler : public IRequestHandler
{
    public:
        HttpRequestHandler();
        virtual ~HttpRequestHandler();

        virtual bool handle_data(int slot_index, ConnectionSlot &slot);
        virtual void handle_close(int slot_index);

    private:
        HttpParser _parser;
        MimeTypes  _mimeTypes;

        bool        _isRequestComplete(const std::string &rawRequest) const;
        bool        _hasHeaderEnd(const std::string &rawRequest, size_t &headerEnd) const;
        std::string _getHeaderValue(const std::string &headerPart, const std::string &key) const;
        std::string _toLower(const std::string &str) const;
        size_t      _stringToSize(const std::string &str) const;

        HttpResponse _buildResponse(const HttpRequest &request);
};

#endif