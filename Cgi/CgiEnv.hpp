#ifndef CGIENV_HPP
# define CGIENV_HPP

#include "../Http/Message/HttpRequest.hpp"
#include "../Router/Router.hpp"
#include <string>
#include <vector>
#include <sstream>

class CgiEnv
{
    private:
        std::vector<std::string> _envStrings;
        std::vector<char *> _envp;

        std::string _toString(size_t value) const;
        void _add(const std::string &key, const std::string &value);

    public:
        CgiEnv();

        char **build(const HttpRequest &request, const RouteResult &route,
                     const std::string &scriptPath, const std::string &pathInfo);
};

#endif
