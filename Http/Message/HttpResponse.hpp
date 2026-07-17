#ifndef HTTPRESPONSE_HPP
#define HTTPRESPONSE_HPP

#include <string>
#include <map>

class HttpResponse
{
private:
    std::string _version;
    int _statusCode;
    std::string _reasonPhrase;
    std::map<std::string, std::string> _headers;
    std::string _body;

public:
    HttpResponse();

    void setVersion(const std::string& version);
    void setStatus(int statusCode);
    void setHeader(const std::string& key, const std::string& value);
    void setBody(const std::string& body);

    const std::string& getVersion() const;
    int getStatusCode() const;
    const std::string& getReasonPhrase() const;
    const std::string& getBody() const;

    std::string getHeader(const std::string& key) const;
    bool hasHeader(const std::string& key) const;

    std::string toString();
    static HttpResponse makeErrorResponse(int statusCode);
    static HttpResponse makeRedirectResponse(int statusCode, const std::string& location);
    static HttpResponse makeMethodNotAllowedResponse(const std::string& allowedMethods);
};

#endif
