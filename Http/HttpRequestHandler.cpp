#include "HttpRequestHandler.hpp"
#include "DefaultConfig.hpp"
#include <iostream>
#include <sstream>
#include <cstdlib>
#include <sys/stat.h>
#include <sys/types.h>
#include <fstream>
#include <ctime>
#include <cstdio>

HttpRequestHandler::HttpRequestHandler(const Config &config) : _router(config), _staticHandler()
{
}

HttpRequestHandler::~HttpRequestHandler()
{
}

std::string HttpRequestHandler::_sizeToString(size_t value) const
{
	std::ostringstream oss;

	oss << value;
	return oss.str();
}

HttpResponse HttpRequestHandler::_makeErrorResponse(int statusCode) const
{
	HttpResponse response;
	std::string filePath;
	std::string body;

	filePath = DefaultConfig::ERROR_ROOT + "/";
	filePath += _sizeToString(static_cast<size_t>(statusCode));
	filePath += ".html";

	if (_fileExists(filePath) && !_isDirectory(filePath))
	{
		if (_readFile(filePath, body))
		{
			response.setStatus(statusCode);
			response.setHeader("Content-Type", "text/html");
			response.setBody(body);
			return response;
		}
	}

	return HttpResponse::makeErrorResponse(statusCode);
}

std::string HttpRequestHandler::_toLower(const std::string &str) const
{
	std::string result;
	size_t i;

	result = str;
	i = 0;
	while (i < result.length())
	{
		if (result[i] >= 'A' && result[i] <= 'Z')
			result[i] = result[i] + 32;
		i++;
	}
	return result;
}

size_t HttpRequestHandler::_stringToSize(const std::string &str) const
{
	size_t result;
	size_t i;

	result = 0;
	i = 0;
	while (i < str.length())
	{
		if (str[i] < '0' || str[i] > '9')
			return 0;
		result = result * 10 + static_cast<size_t>(str[i] - '0');
		i++;
	}
	return result;
}

bool HttpRequestHandler::_hasHeaderEnd(const std::string &rawRequest, size_t &headerEnd) const
{
	headerEnd = rawRequest.find("\r\n\r\n");
	return headerEnd != std::string::npos;
}

std::string HttpRequestHandler::_getHeaderValue(const std::string &headerPart,
												const std::string &key) const
{
	std::istringstream stream(headerPart);
	std::string line;
	std::string lowerKey;
	size_t colonPos;
	std::string headerName;
	std::string headerValue;

	lowerKey = _toLower(key);

	while (std::getline(stream, line))
	{
		if (!line.empty() && line[line.length() - 1] == '\r')
			line.erase(line.length() - 1);

		colonPos = line.find(':');
		if (colonPos == std::string::npos)
			continue;

		headerName = _toLower(line.substr(0, colonPos));
		headerValue = line.substr(colonPos + 1);

		while (!headerValue.empty()
			&& (headerValue[0] == ' ' || headerValue[0] == '\t'))
			headerValue.erase(0, 1);

		while (!headerValue.empty()
			&& (headerValue[headerValue.length() - 1] == ' '
				|| headerValue[headerValue.length() - 1] == '\t'
				|| headerValue[headerValue.length() - 1] == '\r'))
			headerValue.erase(headerValue.length() - 1);

		if (headerName == lowerKey)
			return headerValue;
	}

	return "";
}

bool HttpRequestHandler::_isRequestComplete(const std::string &rawRequest) const
{
	size_t headerEnd;
	std::string headerPart;
	std::string bodyPart;
	std::string contentLength;
	std::string transferEncoding;
	size_t expectedBodySize;

	if (!_hasHeaderEnd(rawRequest, headerEnd))
		return false;

	headerPart = rawRequest.substr(0, headerEnd);
	bodyPart = rawRequest.substr(headerEnd + 4);

	transferEncoding = _toLower(_getHeaderValue(headerPart, "Transfer-Encoding"));
	if (transferEncoding.find("chunked") != std::string::npos)
	{
		if (bodyPart.find("\r\n0\r\n\r\n") != std::string::npos)
			return true;
		if (bodyPart.find("0\r\n\r\n") == 0)
			return true;
		return false;
	}

	contentLength = _getHeaderValue(headerPart, "Content-Length");
	if (!contentLength.empty())
	{
		expectedBodySize = _stringToSize(contentLength);
		return bodyPart.length() >= expectedBodySize;
	}

	return true;
}

bool HttpRequestHandler::_readFile(const std::string &path, std::string &out) const
{
	std::ifstream file;
	std::ostringstream content;

	file.open(path.c_str(), std::ios::in | std::ios::binary);
	if (!file.is_open())
		return false;

	content << file.rdbuf();

	if (file.bad())
	{
		file.close();
		return false;
	}

	file.close();
	out = content.str();
	return true;
}

bool HttpRequestHandler::_isDirectory(const std::string &path) const
{
	struct stat st;

	if (stat(path.c_str(), &st) != 0)
		return false;

	return S_ISDIR(st.st_mode);
}

bool HttpRequestHandler::_fileExists(const std::string &path) const
{
	struct stat st;

	if (stat(path.c_str(), &st) != 0)
		return false;

	return true;
}

std::string HttpRequestHandler::_buildCgiScriptPath(const HttpRequest &request, const RouteResult &route) const
{
	std::string root;
	std::string path;
	std::string locationPath;

	if (route.location && route.location->hasRoot)
		root = route.location->root;
	else if (route.server && route.server->hasRoot)
		root = route.server->root;
	else
		root = "www";

	path = request.getPath();

	if (route.location)
		locationPath = route.location->path;

	if (locationPath != "/" && path.compare(0, locationPath.length(), locationPath) == 0)
		path = path.substr(locationPath.length());

	if (path.empty())
		return root;

	if (root[root.length() - 1] == '/' && path[0] == '/')
		return root + path.substr(1);

	if (root[root.length() - 1] != '/' && path[0] != '/')
		return root + "/" + path;

	return root + path;
}



bool HttpRequestHandler::_writeCgiInput(int fd, const std::string &body) const
{
	size_t written = 0;
	ssize_t n;
	size_t retry = 0;

	while (written < body.size())
	{
		n = write(fd, body.c_str() + written, body.size() - written);
		if (n > 0)
		{
			written += static_cast<size_t>(n);
			retry = 0;
		}
		else
		{
			retry++;
			if (retry > 11000)
			{
				close(fd);
				return false;
			}
			usleep(1000);
		}
	}
	
	close(fd);
	return true;
}

bool HttpRequestHandler::_readCgiOutput(int fd, std::string &output) const
{
	char buffer[3072];
	ssize_t n;
	int retry = 0;
	
	while (true)
	{
		n = read(fd, buffer, sizeof(buffer));
		if (n > 0)
		{
			output.append(buffer, static_cast<size_t>(n));
			retry = 0;
		}
		else if (n == 0)
			break;
		else
		{
			retry++;
			if (retry > 11000)
			{
				close(fd);
				return false;
			}
			usleep(1000);
		}
	}
	close(fd);
	return true;
}

HttpResponse HttpRequestHandler::_makeRawCgiResponse(const std::string &output) const
{
    HttpResponse response;

    response.setStatus(200);
    response.setHeader("Content-Type", "text/plain");
    response.setBody(output);

    return response;
}



HttpResponse HttpRequestHandler::_handleCgi(const HttpRequest &request, const RouteResult &route)
{
	CgiEnv env;
	CgiProcess process;
	std::string scriptPath;
	std::string output;
	char **envp;
	int status;

	if (request.getMethod() != "GET" && request.getMethod() != "POST")
		return _makeErrorResponse(405);

	if (route.location == NULL || route.location->cgiInterpreter.empty())
		return _makeErrorResponse(500);

	scriptPath = _buildCgiScriptPath(request, route);

	if (!_fileExists(scriptPath))
		return _makeErrorResponse(404);

	if (_isDirectory(scriptPath))
		return _makeErrorResponse(403);

	envp = env.build(request, route, scriptPath, "");

	if (!process.start(route.location->cgiInterpreter, scriptPath, envp))
		return _makeErrorResponse(500);
	
	if (!_writeCgiInput(process.getStdinFd() ,request.getBody()))
	{
		close(process.getStdoutFd());
		waitpid(process.getPid(), &status, 0);
		return _makeErrorResponse(500);
	}

	if (!_readCgiOutput(process.getStdoutFd(), output))
        return _makeErrorResponse(500);

	if (waitpid(process.getPid(), &status, 0) == -1)
	{
		close(process.getStdoutFd());
        return _makeErrorResponse(500);
	}

	return _makeRawCgiResponse(output);;
}

bool HttpRequestHandler::_isCgiRequest(const HttpRequest &request, const RouteResult &route) const
{
	std::string path;
	std::string extension;

	if (route.location == NULL)
		return false;

	if (!route.location->hasCGI)
		return false;

	extension = route.location->cgiExtension;
	if (extension.empty())
		return false;

	path = request.getPath();
	if (path.length() < extension.length())
		return false;

	return path.substr(path.length() - extension.length()) == extension;
}

HttpResponse HttpRequestHandler::_buildResponse(const HttpRequest &request)
{
	RouteResult route = _router.route(request);

	if (route.server == NULL)
		return _makeErrorResponse(500);
	
	if (route.location == NULL)
		return _makeErrorResponse(404);

	if (_isCgiRequest(request, route))
		return _handleCgi(request, route);

	if (request.getMethod() == "GET" || request.getMethod() == "DELETE" || request.getMethod() == "POST")
		return _staticHandler.handle(request, route);

	return _makeErrorResponse(405);
}

bool HttpRequestHandler::handle_data(int slot_index, ConnectionSlot &slot)
{
	std::string rawRequest;
	HttpRequest request;
	HttpResponse response;
	std::string rawResponse;

	(void)slot_index;

	rawRequest.assign(slot.readbuffer.begin(), slot.readbuffer.end());

	if (!_isRequestComplete(rawRequest))
		return false;

	try
	{
		request = _parser.parse(rawRequest);
		response = _buildResponse(request);
	}
	catch (const HttpParser::ParseException &e)
	{
		response = _makeErrorResponse(e.getStatusCode());
	}
	catch (const std::exception &)
	{
		response = _makeErrorResponse(500);
	}

	rawResponse = response.toString();

	slot.writebuffer.assign(rawResponse.begin(), rawResponse.end());
	slot.write_position = 0;
	slot.request_complete = true;
	slot.state = ConnectState_WRITING;

	return true;
}

void HttpRequestHandler::handle_close(int slot_index)
{
	std::cout << "[HttpRequestHandler] connection closed: slot["
			  << slot_index << "]" << std::endl;
}
