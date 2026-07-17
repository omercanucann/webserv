#ifndef STATUSCODE_HPP
#define STATUSCODE_HPP

#include <string>

class StatusCode
{
    public:
        static std::string getReasonPhrase(int statusCode);
        static bool isBodyAllowed(int statusCode);
        static bool isRedirect(int statusCode);
        static bool isError(int statusCode);
};

#endif