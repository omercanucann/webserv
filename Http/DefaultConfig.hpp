#ifndef DEFAULTCONFIG_HPP
#define DEFAULTCONFIG_HPP

#include <string>

namespace DefaultConfig //Bu kullanılmayacak daha sonrasında
{
    const std::string ROOT = "www";
    const std::string INDEX = "index.html";
    const std::string UPLOAD_DIR = "www/uploads";
    const std::string ERROR_ROOT = "www/errors";
    const size_t CLIENT_MAX_BODY_SIZE = 1000000;
}

#endif