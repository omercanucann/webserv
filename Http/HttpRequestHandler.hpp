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

        HttpResponse _handleGet(const HttpRequest &request);
        HttpResponse _handlePost(const HttpRequest &request);
        HttpResponse _handleDelete(const HttpRequest &request);

        bool        _isMethodAllowed(const std::string &method) const;
        bool        _isDirectory(const std::string &path) const;
        bool        _fileExists(const std::string &path) const;
        bool        _readFile(const std::string &path, std::string &out) const;
        std::string _buildFilePath(const std::string &requestPath) const;
        std::string _resolveGetPath(const std::string &requestPath) const;
        bool        _containsPathTraversal(const std::string &path) const;
        bool        _ensureDirectory(const std::string &path) const;
        std::string _generateUploadFileName() const;
        bool        _writeFile(const std::string &path, const std::string &content) const;
        bool        _isBodyTooLarge(const HttpRequest &request) const;
        bool        _isUploadPath(const std::string &path) const;
};

#endif