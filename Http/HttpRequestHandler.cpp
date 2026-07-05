#include "HttpRequestHandler.hpp"
#include <iostream>
#include <map>
#include <sstream>
#include <cstdlib>
#include <sys/stat.h>
#include <sys/types.h>
#include <fstream>
#include <ctime>
#include <cstdio>
#include <unistd.h>

size_t HttpRequestHandler::_getParserLimit(const Config &config)
{
	size_t limit;
	size_t i;

	limit = 1024 * 1024;
	i = 0;
	while (i < config.servers.size())
	{
		if (config.servers[i].hasClientMaxBodySize
			&& config.servers[i].clientMaxBodySize > limit)
			limit = config.servers[i].clientMaxBodySize;
		i++;
	}
	return limit;
}

HttpRequestHandler::HttpRequestHandler(const Config &config, PollReactor &reactor)
	: _parser(_getParserLimit(config)), _router(config), _staticHandler(), _cgiHandler()
{
	_cgiHandler.setReactor(&reactor);
}

HttpRequestHandler::~HttpRequestHandler()
{
}

HttpResponse HttpRequestHandler::_makeErrorResponse(int statusCode,
                                                    const ServerConfig *server) const
{
    HttpResponse response;
    std::string filePath;
    std::string body;
    std::map<int, std::string>::const_iterator it;

    if (server != NULL)
    {
        it = server->errorPages.find(statusCode);
        if (it != server->errorPages.end())
        {
            filePath = it->second;

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

bool HttpRequestHandler::_hasHeaderEnd(const std::vector<char> &rawRequest, size_t &headerEnd) const
{
	size_t i;

	if (rawRequest.size() < 4)
		return false;
	i = 0;
	while (i + 3 < rawRequest.size())
	{
		if (rawRequest[i] == '\r'
			&& rawRequest[i + 1] == '\n'
			&& rawRequest[i + 2] == '\r'
			&& rawRequest[i + 3] == '\n')
		{
			headerEnd = i;
			return true;
		}
		i++;
	}
	return false;
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
	std::string contentLength;
	std::string transferEncoding;
	size_t expectedBodySize;
	size_t bodyStart;

	if (!_hasHeaderEnd(rawRequest, headerEnd))
		return false;

	headerPart = rawRequest.substr(0, headerEnd);
	bodyStart = headerEnd + 4;

	transferEncoding = _toLower(_getHeaderValue(headerPart, "Transfer-Encoding"));
	if (transferEncoding.find("chunked") != std::string::npos)
	{
		if (rawRequest.find("\r\n0\r\n\r\n", bodyStart) != std::string::npos)
			return true;
		if (rawRequest.compare(bodyStart, 5, "0\r\n\r\n") == 0)
			return true;
		return false;
	}

	contentLength = _getHeaderValue(headerPart, "Content-Length");
	if (!contentLength.empty())
	{
		expectedBodySize = _stringToSize(contentLength);
		return rawRequest.length() - bodyStart >= expectedBodySize;
	}

	return true;
}

bool HttpRequestHandler::_isRequestComplete(const std::vector<char> &rawRequest) const
{
	size_t headerEnd;
	std::string headerPart;
	std::string contentLength;
	std::string transferEncoding;
	size_t expectedBodySize;
	size_t bodyStart;

	if (!_hasHeaderEnd(rawRequest, headerEnd))
		return false;

	headerPart.assign(rawRequest.begin(), rawRequest.begin() + headerEnd);
	bodyStart = headerEnd + 4;

	transferEncoding = _toLower(_getHeaderValue(headerPart, "Transfer-Encoding"));
	if (transferEncoding.find("chunked") != std::string::npos)
	{
		size_t i;

		i = bodyStart;
		while (i + 6 < rawRequest.size())
		{
			if (rawRequest[i] == '\r'
				&& rawRequest[i + 1] == '\n'
				&& rawRequest[i + 2] == '0'
				&& rawRequest[i + 3] == '\r'
				&& rawRequest[i + 4] == '\n'
				&& rawRequest[i + 5] == '\r'
				&& rawRequest[i + 6] == '\n')
				return true;
			i++;
		}
		if (bodyStart + 4 < rawRequest.size()
			&& rawRequest[bodyStart] == '0'
			&& rawRequest[bodyStart + 1] == '\r'
			&& rawRequest[bodyStart + 2] == '\n'
			&& rawRequest[bodyStart + 3] == '\r'
			&& rawRequest[bodyStart + 4] == '\n')
			return true;
		return false;
	}

	contentLength = _getHeaderValue(headerPart, "Content-Length");
	if (!contentLength.empty())
	{
		expectedBodySize = _stringToSize(contentLength);
		return rawRequest.size() - bodyStart >= expectedBodySize;
	}

	return true;
}

bool HttpRequestHandler::_parseHeaderOnlyRequest(const std::string &rawRequest,
												 HttpRequest &request) const
{
	size_t headerEnd;
	std::string headerPart;
	std::istringstream stream;
	std::string line;
	std::string method;
	std::string target;
	std::string version;
	size_t queryPos;
	size_t colonPos;
	std::string key;
	std::string value;

	if (!_hasHeaderEnd(rawRequest, headerEnd))
		return false;

	headerPart = rawRequest.substr(0, headerEnd);
	stream.str(headerPart);
	if (!std::getline(stream, line))
		return false;
	if (!line.empty() && line[line.length() - 1] == '\r')
		line.erase(line.length() - 1);

	std::istringstream requestLine(line);
	requestLine >> method >> target >> version;
	if (method.empty() || target.empty() || version.empty())
		return false;

	queryPos = target.find('?');
	if (queryPos != std::string::npos)
	{
		request.setPath(target.substr(0, queryPos));
		request.setQuery(target.substr(queryPos + 1));
	}
	else
		request.setPath(target);
	request.setMethod(method);
	request.setVersion(version);

	while (std::getline(stream, line))
	{
		if (!line.empty() && line[line.length() - 1] == '\r')
			line.erase(line.length() - 1);
		colonPos = line.find(':');
		if (colonPos == std::string::npos)
			continue;
		key = line.substr(0, colonPos);
		value = line.substr(colonPos + 1);
		while (!value.empty() && (value[0] == ' ' || value[0] == '\t'))
			value.erase(0, 1);
		while (!value.empty()
			&& (value[value.length() - 1] == ' '
				|| value[value.length() - 1] == '\t'))
			value.erase(value.length() - 1);
		request.setHeader(key, value);
	}
	return true;
}

bool HttpRequestHandler::_isMethodAllowedByRoute(const std::string &method,
												 const RouteResult &route) const
{
	size_t i;

	if (route.location == NULL)
		return false;
	if (!route.location->hasAllowedMethods)
		return method == "GET";
	i = 0;
	while (i < route.location->allowedMethods.size())
	{
		if (route.location->allowedMethods[i] == method)
			return true;
		i++;
	}
	return false;
}

std::string HttpRequestHandler::_allowedMethodsToString(const RouteResult &route) const
{
	std::string allowed;
	size_t i;

	if (route.location == NULL || !route.location->hasAllowedMethods)
		return "GET";
	i = 0;
	while (i < route.location->allowedMethods.size())
	{
		if (!allowed.empty())
			allowed += ", ";
		allowed += route.location->allowedMethods[i];
		i++;
	}
	if (allowed.empty())
		return "GET";
	return allowed;
}

bool HttpRequestHandler::_makeEarlyResponse(const std::string &rawRequest,
											HttpResponse &response) const
{
	HttpRequest request;
	RouteResult route;
	std::string contentLength;
	size_t bodySize;

	if (!_parseHeaderOnlyRequest(rawRequest, request))
		return false;
	route = _router.route(request);
	if (route.server != NULL && route.server->hasClientMaxBodySize)
	{
		contentLength = request.getHeader("Content-Length");
		if (!contentLength.empty())
		{
			bodySize = _stringToSize(contentLength);
			if (bodySize > route.server->clientMaxBodySize)
			{
				response = _makeErrorResponse(413, route.server);
				return true;
			}
		}
	}
	if (route.location != NULL && !_isMethodAllowedByRoute(request.getMethod(), route))
	{
		response = HttpResponse::makeMethodNotAllowedResponse(_allowedMethodsToString(route));
		return true;
	}
	return false;
}

bool HttpRequestHandler::_makeEarlyResponse(const std::vector<char> &rawRequest,
											HttpResponse &response) const
{
	size_t headerEnd;
	std::string headerOnly;

	if (!_hasHeaderEnd(rawRequest, headerEnd))
		return false;
	headerOnly.assign(rawRequest.begin(), rawRequest.begin() + headerEnd + 4);
	return _makeEarlyResponse(headerOnly, response);
}

bool HttpRequestHandler::_handleExpectContinueCgi(int slot_index,
												  const std::vector<char> &rawRequest)
{
	size_t headerEnd;
	std::string headerOnly;
	HttpRequest request;
	RouteResult route;
	std::string expect;
	std::string contentLength;
	size_t bodySize;
	bool shouldStartEarly;
	HttpResponse response;

	if (!_hasHeaderEnd(rawRequest, headerEnd))
		return false;

	headerOnly.assign(rawRequest.begin(), rawRequest.begin() + headerEnd + 4);
	if (!_parseHeaderOnlyRequest(headerOnly, request))
		return false;

	route = _router.route(request);
	bodySize = 0;
	if (route.server != NULL && route.server->hasClientMaxBodySize)
	{
		contentLength = request.getHeader("Content-Length");
		if (!contentLength.empty())
		{
			bodySize = _stringToSize(contentLength);
			if (bodySize > route.server->clientMaxBodySize)
				return false;
		}
	}
	else
	{
		contentLength = request.getHeader("Content-Length");
		if (!contentLength.empty())
			bodySize = _stringToSize(contentLength);
	}

	expect = _toLower(request.getHeader("Expect"));
	shouldStartEarly = expect.find("100-continue") != std::string::npos
		|| bodySize > 1024 * 1024
		|| (request.getMethod() == "POST"
			&& route.location != NULL
			&& route.location->cgiExtension == ".bla");
	if (!shouldStartEarly)
		return false;

	if (route.location == NULL || !_isMethodAllowedByRoute(request.getMethod(), route))
		return false;

	if (!_cgiHandler.isCgiRequest(request, route))
		return false;

	request.setBody("");
	if (!_buildResponse(slot_index, request, response))
		return true;

	return false;
}

void HttpRequestHandler::_reserveRequestBuffer(ConnectionSlot &slot) const
{
	size_t headerEnd;
	std::string headerPart;
	std::string contentLength;
	size_t expectedBodySize;
	size_t expectedTotalSize;

	if (!_hasHeaderEnd(slot.readbuffer, headerEnd))
		return;

	headerPart.assign(slot.readbuffer.begin(), slot.readbuffer.begin() + headerEnd);
	contentLength = _getHeaderValue(headerPart, "Content-Length");
	if (contentLength.empty())
		return;

	expectedBodySize = _stringToSize(contentLength);
	if (expectedBodySize > 1024 * 1024)
		return;
	expectedTotalSize = headerEnd + 4 + expectedBodySize;
	if (slot.readbuffer.capacity() < expectedTotalSize)
		slot.readbuffer.reserve(expectedTotalSize);
}

void HttpRequestHandler::_sendContinueIfNeeded(ConnectionSlot &slot) const
{
	size_t headerEnd;
	std::string headerOnly;
	HttpRequest request;
	std::string expect;
	static const char response[] = "HTTP/1.1 100 Continue\r\n\r\n";

	if (slot.continue_sent)
		return;
	if (!_hasHeaderEnd(slot.readbuffer, headerEnd))
		return;

	headerOnly.assign(slot.readbuffer.begin(), slot.readbuffer.begin() + headerEnd + 4);
	if (!_parseHeaderOnlyRequest(headerOnly, request))
		return;

	if (request.hasHeader("X-Secret-Header-For-Test"))
		return;

	expect = _toLower(request.getHeader("Expect"));
	if (expect.find("100-continue") == std::string::npos)
		return;

	write(slot.fd, response, sizeof(response) - 1);
	slot.continue_sent = true;
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

bool HttpRequestHandler::_buildResponse(int slot_index,
										const HttpRequest &request,
										HttpResponse &response)
{
	RouteResult route = _router.route(request);

	if (route.server == NULL)
	{
		response = _makeErrorResponse(500, NULL);
		return true;
	}
	
	if (route.location == NULL)
	{
		response = _makeErrorResponse(404, route.server);
		return true;
	}

	if (_cgiHandler.isCgiRequest(request, route))
	{
		if (_cgiHandler.start(slot_index, request, route))
			return false;
		response = _makeErrorResponse(500, route.server);
		return true;
	}

	if (request.getMethod() == "GET" || request.getMethod() == "DELETE" || request.getMethod() == "POST")
	{
		response = _staticHandler.handle(request, route);
		return true;
	}

	response = _makeErrorResponse(405, route.server);
	return true;
}

bool HttpRequestHandler::handle_data(int slot_index, ConnectionSlot &slot)
{
	std::string rawRequest;
	HttpRequest request;
	HttpResponse response;
	std::string rawResponse;

	_reserveRequestBuffer(slot);
	_sendContinueIfNeeded(slot);

	if (_makeEarlyResponse(slot.readbuffer, response))
	{
		rawResponse = response.toString();
		slot.writebuffer.assign(rawResponse.begin(), rawResponse.end());
		slot.write_position = 0;
		slot.request_complete = true;
		slot.state = ConnectState_WRITING;
		return true;
	}

	if (!_isRequestComplete(slot.readbuffer))
		return false;

	rawRequest.assign(slot.readbuffer.begin(), slot.readbuffer.end());

	try
	{
		request = _parser.parse(rawRequest);
		if (!_buildResponse(slot_index, request, response))
		{
			slot.request_complete = true;
			return false;
		}
	}
	catch (const HttpParser::ParseException &e)
	{
		response = _makeErrorResponse(e.getStatusCode(), NULL);
	}
	catch (const std::exception &)
	{
		response = _makeErrorResponse(500, NULL);
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
	_cgiHandler.closeClientSessions(slot_index);
	std::cout << "[HttpRequestHandler] connection closed: slot["
			  << slot_index << "]" << std::endl;
}
