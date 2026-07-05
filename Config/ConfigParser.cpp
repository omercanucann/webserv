#include "ConfigParser.hpp"

ConfigParser::ParseException::ParseException(const std::string &message)
	: _message(message)
{

}

ConfigParser::ParseException::~ParseException() throw()
{

}
const char *ConfigParser::ParseException::what() const throw()
{
	return _message.c_str();
}

void ConfigParser::parseLocationBlock(const std::vector<std::string> &tokens, size_t &i, LocationConfig &loc)
{
	while (i < tokens.size() && tokens[i] != "}")
	{
		if (tokens[i] == "allow_methods")
		{
			i++;
			while (i < tokens.size() && tokens[i] != ";")
			{
				loc.allowedMethods.push_back(tokens[i]);
				i++;
			}
			if (i >= tokens.size() || loc.allowedMethods.empty())
                throw ParseException("Expected ';' after allow_methods");
            loc.hasAllowedMethods = true;
            i++;
		}
		else if (tokens[i] == "root")
		{
			i++;
            if (i + 1 >= tokens.size() || tokens[i + 1] != ";")
                throw ParseException("Invalid root syntax inside location");
			loc.root = tokens[i];
			loc.hasRoot = true;
			i += 2;
		}
		else if (tokens[i] == "index")
		{
			i++;
            if (i + 1 >= tokens.size() || tokens[i + 1] != ";")
                throw ParseException("Invalid index syntax inside location");
			loc.index = tokens[i];
			loc.hasIndex = true;
			i += 2;
		}
		else if (tokens[i] == "client_max_body_size")
		{
			i++;
            if (i + 1 >= tokens.size() || tokens[i + 1] != ";")
                throw ParseException("Invalid client_max_body_size syntax inside location");
			loc.clientMaxBodySize =
				static_cast<size_t>(std::strtoul(tokens[i].c_str(), NULL, 10));
			loc.hasClientMaxBodySize = true;
			i += 2;
		}
		else if (tokens[i] == "autoindex")
		{
			i++;
            if (i + 1 >= tokens.size() || tokens[i + 1] != ";")
                throw ParseException("Invalid autoindex syntax inside location");
			if (tokens[i] == "on")
				loc.autoindex = true;
			else if (tokens[i] == "off")
				loc.autoindex = false;
			else
				throw ParseException("Invalid autoindex value: " + tokens[i]);
			loc.hasAutoIndex = true;
			i += 2;
		}
		else if (tokens[i] == "upload_enable")
        {
			i++;
            if (i + 1 >= tokens.size() || tokens[i + 1] != ";")
                throw ParseException("Invalid upload_enable syntax");
            if (tokens[i] == "on")
                loc.uploadEnabled = true;
            else if (tokens[i] == "off")
                loc.uploadEnabled = false;
            else
                throw ParseException("Invalid upload_enable value: " + tokens[i]);
            loc.hasUploadEnabled = true;
            i += 2;
        }
		else if (tokens[i] == "upload_path")
        {
			i++;
            if (i + 1 >= tokens.size() || tokens[i + 1] != ";")
                throw ParseException("Invalid upload_path syntax");
            loc.uploadPath = tokens[i];
            loc.hasUploadPath = true;
            i += 2;
        }
		else if (tokens[i] == "return")
        {
			i++;
			std::vector<std::string> retTokens;

			while (i < tokens.size() && tokens[i] != ";")
			{
				retTokens.push_back(tokens[i]);
				i++;
			}
			if (i >= tokens.size() || retTokens.empty())
                throw ParseException("Invalid return syntax");
            i++;

			if (retTokens.size() == 1)
			{
				loc.redirectStatus = 302;
				loc.redirectTarget = retTokens[0];
			}
			else if (retTokens.size() == 2)
			{
				loc.redirectStatus = std::atoi(retTokens[0].c_str());
				loc.redirectTarget = retTokens[1];
			}
			else
				throw ParseException("Invalid return arguments");
			
			loc.hasRedirect = true;
        }
		else if (tokens[i] == "cgi_extension")
        {
			i++;
            if (i + 1 >= tokens.size() || tokens[i + 1] != ";")
                throw ParseException("Invalid cgi_extension syntax");
            loc.cgiExtension = tokens[i];
            loc.hasCGI = true;
            i += 2;
        }
		else if (tokens[i] == "cgi_interpreter")
        {
			i++;
            if (i + 1 >= tokens.size() || tokens[i + 1] != ";")
                throw ParseException("Invalid cgi_interpreter syntax");
            loc.cgiInterpreter = tokens[i];
            loc.hasCGI = true;
            i += 2;
        }
		else
			throw ParseException("Unknown directive inside location block: " + tokens[i]);
		
	}
	if (i >= tokens.size() || tokens[i] != "}")
		throw ParseException("Unclosed location block");
	i++;
}

void ConfigParser::ParseServerBlock(std::vector<std::string> &tokens, size_t &i, ServerConfig &server)
{
	while (i < tokens.size() && tokens[i] != "}")
	{
		if (tokens[i] == "listen")
		{
			i++;
			if (i + 1 >= tokens.size() || tokens[i + 1] != ";")
				throw ParseException("Invalid listen syntax");

			std::string listenStr = tokens[i];
			std::string portStr;
			size_t p;

			std::string::size_type colonPos = listenStr.find(":");
			if (colonPos == std::string::npos)
			{
				server.listenHost = "0.0.0.0";
				portStr = listenStr;
			}
			else
			{
				server.listenHost = listenStr.substr(0, colonPos);
				portStr = listenStr.substr(colonPos + 1);
			}

			if (server.listenHost != "0.0.0.0"
				&& server.listenHost != "127.0.0.1"
				&& server.listenHost != "localhost")
				throw ParseException("Invalid listen host: " + server.listenHost);

			if (portStr.empty())
				throw ParseException("Invalid listen port: " + listenStr);

			p = 0;
			while (p < portStr.length())
			{
				if (portStr[p] < '0' || portStr[p] > '9')
					throw ParseException("Invalid listen port: " + listenStr);
				p++;
			}

			server.listenPort = std::atoi(portStr.c_str());

			if (server.listenPort <= 0 || server.listenPort > 65535)
				throw ParseException("Invalid listen port: " + listenStr);

			server.hasListen = true;
			i += 2;
		}
		else if (tokens[i] == "root")
		{
			i++;
			if (i + 1 >= tokens.size() || tokens[i + 1] != ";")
				throw ParseException("Invalid root syntax");

			server.root = tokens[i];
			server.hasRoot = true;
			i += 2;
		}
		else if (tokens[i] == "index")
		{
			i++;
			if (i + 1 >= tokens.size() || tokens[i + 1] != ";")
				throw ParseException("Invalid index syntax");

			server.index = tokens[i];
			server.hasIndex = true;
			i += 2;
		}
		else if (tokens[i] == "client_max_body_size")
		{
			i++;
			if (i + 1 >= tokens.size() || tokens[i + 1] != ";")
				throw ParseException("Invalid client_max_body_size syntax");

			server.clientMaxBodySize =
				static_cast<size_t>(std::strtoul(tokens[i].c_str(), NULL, 10));

			server.hasClientMaxBodySize = true;
			i += 2;
		}
		else if (tokens[i] == "error_page")
		{
			i++;
			std::vector<std::string> errTokens;

			while (i < tokens.size() && tokens[i] != ";")
			{
				errTokens.push_back(tokens[i]);
				i++;
			}

			if (i >= tokens.size() || errTokens.size() < 2)
				throw ParseException("Invalid error_page syntax");

			std::string errorPath = errTokens.back();

			for (size_t a = 0; a < errTokens.size() - 1; ++a)
			{
				int code = std::atoi(errTokens[a].c_str());

				if (code < 300 || code > 599)
					throw ParseException("Invalid error code in error_page: " + errTokens[a]);

				server.errorPages[code] = errorPath;
			}

			i++;
		}
		else if (tokens[i] == "location")
		{
			i++;
			if (i >= tokens.size())
				throw ParseException("Expected path after location keyword");

			std::string path = tokens[i];
			i++;

			if (i >= tokens.size() || tokens[i] != "{")
				throw ParseException("Expected '{' for location block");

			i++;

			LocationConfig loc;
			loc.path = path;

			parseLocationBlock(tokens, i, loc);
			server.locations.push_back(loc);
		}
		else
		{
			throw ParseException("Unknown directive in server block: " + tokens[i]);
		}
	}

	if (i >= tokens.size() || tokens[i] != "}")
		throw ParseException("Unclosed server block");

	if (!server.hasListen)
    	throw ParseException("Missing listen directive in server block");
	i++;
}
std::vector<std::string> ConfigParser::tokenizeConfig(std::string &conf)
{
	std::vector<std::string> tokens;
	std::string token;
	size_t i = 0;

	while (i < conf.length())
	{
		if (conf[i] == '#')
		{
			while (i < conf.length() && conf[i] != '\n')
				i++; 
		}
		else if (std::isspace(static_cast<unsigned char>(conf[i])))
		{
			if (!token.empty())
			{
				tokens.push_back(token);
				token.clear();
			}
		}
		else if (conf[i] == '{' || conf[i] == '}' || conf[i] == ';')
		{
			if (!token.empty())
			{
				tokens.push_back(token);
				token.clear();
			}
			tokens.push_back(std::string(1, conf[i]));
		}
		else
		{
			token.push_back(conf[i]);
		}
		i++;
	}
	if (!token.empty())
		tokens.push_back(token);
	return tokens;
}

Config ConfigParser::parseConfig(const std::string &path)
{
	Config conf;
	conf.configPath = path;

	std::string::size_type pos = path.find_last_of("/\\");
	if (pos == std::string::npos)
		conf.configName = path;
	else
		conf.configName = path.substr(pos + 1);

	std::string content = ConfigRead(path);
	std::vector<std::string> tokens = tokenizeConfig(content);

	size_t i = 0;

	while (i < tokens.size())
	{
		if (tokens[i] == "server")
		{
			i++;
			if (i >= tokens.size() || tokens[i] != "{")
				throw ParseException("Expected '{' after server keyword");
			i++;

			ServerConfig server;
			ParseServerBlock(tokens, i, server);
			conf.servers.push_back(server);
		}
		else
			throw ParseException("Unexpected token outside server block: " + tokens[i]);
	}
	if (conf.servers.empty())
		throw ParseException("No server block found in config file");

	return conf;
}

std::string ConfigParser::ConfigRead(const std::string &pathconf)
{
	std::ifstream file(pathconf.c_str());
	if (!file.is_open())
		throw ParseException("Config file could not be opened: " + pathconf);
	
	std::stringstream buffer;
	buffer << file.rdbuf();
	return buffer.str();
}
