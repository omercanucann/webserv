#ifndef LOCATION_CONFIG_HPP
# define LOCATION_CONFIG_HPP

#include <string>
#include <vector>

struct LocationConfig
{
    std::string path;
    std::vector<std::string> allowedMethods;
    bool hasAllowedMethods;

    std::string root;
    bool hasRoot;

    std::string index;
    bool hasIndex;

    size_t clientMaxBodySize;
    bool hasClientMaxBodySize;

    bool autoindex;
    bool hasAutoIndex;

    int redirectStatus;
    bool hasRedirect;
    std::string redirectTarget;
    
    bool hasCGI;
    std::string cgiExtension;
    std::string cgiInterpreter;

    bool uploadEnabled;
    std::string uploadPath;
    bool hasUploadEnabled;
    bool hasUploadPath;

    LocationConfig():
        hasAllowedMethods(false),
        hasRoot(false),
        hasIndex(false),
        clientMaxBodySize(0),
        hasClientMaxBodySize(false),
        autoindex(false),
        hasAutoIndex(false),
        redirectStatus(0),
        hasRedirect(false),
        hasCGI(false),
        uploadEnabled(false),
        hasUploadEnabled(false),
        hasUploadPath(false)
    {}
};



#endif
