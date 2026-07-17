#ifndef HTTPPARSER_HPP
#define HTTPPARSER_HPP

#include <string>
#include <exception>
#include "HttpRequest.hpp"

class HttpParser
{
    public:
        class ParseException : public std::exception
        {
            private:
                int _statusCode;
                std::string _message;

            public:
                ParseException(int statusCode, const std::string& message);
                virtual ~ParseException() throw();

                int getStatusCode() const;
                virtual const char* what() const throw();
        };

    private:
        size_t _maxBodySize;

        bool isTokenChar(char c) const;
        bool isValidMethodToken(const std::string& method) const;
        bool isValidHeaderName(const std::string& key) const;
        bool isOnlyDigits(const std::string& str) const;

        size_t stringToSize(const std::string& str) const;
        size_t hexToSize(const std::string& str) const;

        void parseRequestLine(const std::string& line, HttpRequest& request) const;
        void parsePathAndQuery(const std::string& target, HttpRequest& request) const;
        void parseHeaderLine(const std::string& line, HttpRequest& request) const;

        std::string decodeChunkedBody(const std::string& body) const;
        void validateBodyRules(HttpRequest& request, const std::string& rawBody) const;

    public:
        HttpParser();
        HttpParser(size_t maxBodySize);

        HttpRequest parse(const std::string& rawRequest) const;
        HttpRequest parseHeaders(const std::string& rawRequest) const;
};

#endif
