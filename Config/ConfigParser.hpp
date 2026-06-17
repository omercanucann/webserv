#ifndef CONFIGPARSER_HPP
# define CONFIGPARSER_HPP

#include <string>
#include "Config.hpp"
#include <fstream>
#include <sstream>
#include <exception>

struct ConfigParser
{
    class ParseException : public std::exception
    {
        private:
            std::string _message;
        public:
            ParseException(const std::string &message);
            virtual ~ParseException() throw();
            virtual const char *what() const throw();
    };

    Config parseConfig(const std::string &path);
    std::string ConfigRead(const std::string &pathconf);

    std::vector<std::string> tokenizeConfig(std::string &conf);
    void ParseServerBlock(std::vector<std::string> &tokens, size_t &i, ServerConfig &server);
    void parseLocationBlock(const std::vector<std::string> &tokens, size_t &i, LocationConfig &loc);
};


#endif