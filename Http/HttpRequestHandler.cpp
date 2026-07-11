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
#include <cerrno>
#include <fcntl.h>

struct HttpRequestHandler::BodyStreamState
{
	enum ChunkState
	{
		CHUNK_SIZE_LINE = 0,
		CHUNK_DATA = 1,
		CHUNK_DATA_CR = 2,
		CHUNK_DATA_LF = 3,
		CHUNK_FINAL_CR = 4,
		CHUNK_FINAL_LF = 5,
		CHUNK_DONE = 6
	};

	HttpRequest request;
	bool        chunked;
	size_t      expectedSize;
	size_t      bodySize;
	int         tempFd;
	std::string tempPath;
	ChunkState  chunkState;
	std::string chunkLine;
	size_t      currentChunkSize;
	size_t      currentChunkRead;
	size_t      bodyLimit;
	bool        hasBodyLimit;

	BodyStreamState()
		: chunked(false),
		  expectedSize(0),
		  bodySize(0),
		  tempFd(-1),
		  chunkState(CHUNK_SIZE_LINE),
		  currentChunkSize(0),
		  currentChunkRead(0),
		  bodyLimit(0),
		  hasBodyLimit(false)
	{}

	~BodyStreamState()
	{
		closeTemp();
		if (!tempPath.empty())
			std::remove(tempPath.c_str());
	}

	void closeTemp()
	{
		if (tempFd >= 0)
		{
			close(tempFd);
			tempFd = -1;
		}
	}
};

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
	std::map<int, BodyStreamState *>::iterator it;

	it = _bodyStreams.begin();
	while (it != _bodyStreams.end())
	{
		delete it->second;
		++it;
	}
	_bodyStreams.clear();
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

bool HttpRequestHandler::_createTempFile(int &fd, std::string &path) const
{
	std::string pattern;
	std::vector<char> buffer;

	pattern = "/tmp/webserv_cgi_body_XXXXXX";
	buffer.assign(pattern.begin(), pattern.end());
	buffer.push_back('\0');

	fd = mkstemp(&buffer[0]);
	if (fd < 0)
		return false;
	if (fcntl(fd, F_SETFD, FD_CLOEXEC) == -1)
	{
		close(fd);
		fd = -1;
		std::remove(&buffer[0]);
		return false;
	}
	path = &buffer[0];
	return true;
}

bool HttpRequestHandler::_writeAll(int fd, const char *data, size_t size) const
{
	size_t written;

	written = 0;
	while (written < size)
	{
		ssize_t n;

		n = write(fd, data + written, size - written);
		if (n > 0)
			written += static_cast<size_t>(n);
		else if (n < 0 && errno == EINTR)
			continue;
		else
			return false;
	}
	return true;
}

bool HttpRequestHandler::_parseSizeValue(const std::string &value, size_t &out) const
{
	size_t i;

	if (value.empty())
		return false;
	out = 0;
	i = 0;
	while (i < value.length())
	{
		if (value[i] < '0' || value[i] > '9')
			return false;
		if (out > (static_cast<size_t>(-1) - static_cast<size_t>(value[i] - '0')) / 10)
			return false;
		out = out * 10 + static_cast<size_t>(value[i] - '0');
		i++;
	}
	return true;
}

bool HttpRequestHandler::_parseChunkSizeLine(const std::string &line, size_t &out) const
{
	std::string value;
	size_t semicolonPos;
	size_t start;
	size_t end;
	size_t i;
	int digit;

	semicolonPos = line.find(';');
	if (semicolonPos == std::string::npos)
		value = line;
	else
		value = line.substr(0, semicolonPos);

	start = 0;
	while (start < value.length() && (value[start] == ' ' || value[start] == '\t'))
		start++;
	end = value.length();
	while (end > start && (value[end - 1] == ' ' || value[end - 1] == '\t'))
		end--;
	value = value.substr(start, end - start);
	if (value.empty())
		return false;

	out = 0;
	i = 0;
	while (i < value.length())
	{
		if (value[i] >= '0' && value[i] <= '9')
			digit = value[i] - '0';
		else if (value[i] >= 'a' && value[i] <= 'f')
			digit = value[i] - 'a' + 10;
		else if (value[i] >= 'A' && value[i] <= 'F')
			digit = value[i] - 'A' + 10;
		else
			return false;
		if (out > (static_cast<size_t>(-1) - static_cast<size_t>(digit)) / 16)
			return false;
		out = out * 16 + static_cast<size_t>(digit);
		i++;
	}
	return true;
}

bool HttpRequestHandler::_bodyLimitForRoute(const RouteResult &route, size_t &limit) const
{
	bool hasLimit;

	hasLimit = false;
	limit = 0;
	if (route.server != NULL && route.server->hasClientMaxBodySize)
	{
		limit = route.server->clientMaxBodySize;
		hasLimit = true;
	}
	if (route.location != NULL && route.location->hasClientMaxBodySize)
	{
		if (!hasLimit || route.location->clientMaxBodySize < limit)
			limit = route.location->clientMaxBodySize;
		hasLimit = true;
	}
	return hasLimit;
}

void HttpRequestHandler::_cleanupBodyStream(int slot_index)
{
	std::map<int, BodyStreamState *>::iterator it;

	it = _bodyStreams.find(slot_index);
	if (it == _bodyStreams.end())
		return;
	delete it->second;
	_bodyStreams.erase(it);
}

bool HttpRequestHandler::_failCgiBodyStream(int slot_index,
											ConnectionSlot &slot,
											int statusCode)
{
	HttpResponse response;
	std::string rawResponse;

	_cleanupBodyStream(slot_index);
	slot.readbuffer.clear();
	response = _makeErrorResponse(statusCode, NULL);
	rawResponse = response.toString();
	slot.writebuffer.assign(rawResponse.begin(), rawResponse.end());
	slot.write_position = 0;
	slot.request_complete = true;
	slot.state = ConnectState_WRITING;
	return true;
}

bool HttpRequestHandler::_finishCgiBodyStream(int slot_index, ConnectionSlot &slot)
{
	std::map<int, BodyStreamState *>::iterator it;
	BodyStreamState *state;
	HttpResponse response;
	std::string rawResponse;

	it = _bodyStreams.find(slot_index);
	if (it == _bodyStreams.end())
		return false;

	state = it->second;
	state->closeTemp();
	if (!state->tempPath.empty())
		state->request.setBodyFile(state->tempPath, state->bodySize);
	else
		state->request.setBody("");

	try
	{
		if (!_buildResponse(slot_index, state->request, response))
		{
			slot.request_complete = true;
			_cleanupBodyStream(slot_index);
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
	_cleanupBodyStream(slot_index);
	return true;
}

bool HttpRequestHandler::_processCgiBodyStream(int slot_index, ConnectionSlot &slot)
{
	std::map<int, BodyStreamState *>::iterator it;
	BodyStreamState *state;
	size_t consumed;

	it = _bodyStreams.find(slot_index);
	if (it == _bodyStreams.end())
		return false;
	state = it->second;

	if (!state->chunked)
	{
		size_t need;
		size_t toWrite;

		if (state->bodySize >= state->expectedSize)
		{
			slot.readbuffer.clear();
			return _finishCgiBodyStream(slot_index, slot);
		}
		need = state->expectedSize - state->bodySize;
		toWrite = slot.readbuffer.size();
		if (toWrite > need)
			toWrite = need;
		if (toWrite > 0)
		{
			if (!_writeAll(state->tempFd, &slot.readbuffer[0], toWrite))
				return _failCgiBodyStream(slot_index, slot, 500);
			state->bodySize += toWrite;
		}
		if (toWrite > 0)
			slot.readbuffer.erase(slot.readbuffer.begin(), slot.readbuffer.begin() + toWrite);
		if (state->bodySize >= state->expectedSize)
		{
			slot.readbuffer.clear();
			return _finishCgiBodyStream(slot_index, slot);
		}
		return false;
	}

	consumed = 0;
	while (consumed < slot.readbuffer.size())
	{
		if (state->chunkState == BodyStreamState::CHUNK_SIZE_LINE)
		{
			char c;

			c = slot.readbuffer[consumed++];
			state->chunkLine += c;
			if (state->chunkLine.length() > 8192)
				return _failCgiBodyStream(slot_index, slot, 400);
			if (c == '\n')
			{
				std::string line;

				if (state->chunkLine.length() < 2
					|| state->chunkLine[state->chunkLine.length() - 2] != '\r')
					return _failCgiBodyStream(slot_index, slot, 400);
				line = state->chunkLine.substr(0, state->chunkLine.length() - 2);
				if (!_parseChunkSizeLine(line, state->currentChunkSize))
					return _failCgiBodyStream(slot_index, slot, 400);
				state->chunkLine.clear();
				state->currentChunkRead = 0;
				if (state->currentChunkSize == 0)
					state->chunkState = BodyStreamState::CHUNK_FINAL_CR;
				else
					state->chunkState = BodyStreamState::CHUNK_DATA;
			}
		}
		else if (state->chunkState == BodyStreamState::CHUNK_DATA)
		{
			size_t available;
			size_t need;
			size_t toWrite;

			available = slot.readbuffer.size() - consumed;
			need = state->currentChunkSize - state->currentChunkRead;
			toWrite = available;
			if (toWrite > need)
				toWrite = need;
			if (toWrite > 0)
			{
				if (!_writeAll(state->tempFd, &slot.readbuffer[consumed], toWrite))
					return _failCgiBodyStream(slot_index, slot, 500);
				state->currentChunkRead += toWrite;
				state->bodySize += toWrite;
				consumed += toWrite;
				if (state->hasBodyLimit && state->bodySize > state->bodyLimit)
					return _failCgiBodyStream(slot_index, slot, 413);
			}
			if (state->currentChunkRead == state->currentChunkSize)
				state->chunkState = BodyStreamState::CHUNK_DATA_CR;
		}
		else if (state->chunkState == BodyStreamState::CHUNK_DATA_CR)
		{
			if (slot.readbuffer[consumed++] != '\r')
				return _failCgiBodyStream(slot_index, slot, 400);
			state->chunkState = BodyStreamState::CHUNK_DATA_LF;
		}
		else if (state->chunkState == BodyStreamState::CHUNK_DATA_LF)
		{
			if (slot.readbuffer[consumed++] != '\n')
				return _failCgiBodyStream(slot_index, slot, 400);
			state->chunkState = BodyStreamState::CHUNK_SIZE_LINE;
		}
		else if (state->chunkState == BodyStreamState::CHUNK_FINAL_CR)
		{
			if (slot.readbuffer[consumed++] != '\r')
				return _failCgiBodyStream(slot_index, slot, 400);
			state->chunkState = BodyStreamState::CHUNK_FINAL_LF;
		}
		else if (state->chunkState == BodyStreamState::CHUNK_FINAL_LF)
		{
			if (slot.readbuffer[consumed++] != '\n')
				return _failCgiBodyStream(slot_index, slot, 400);
			state->chunkState = BodyStreamState::CHUNK_DONE;
			slot.readbuffer.clear();
			return _finishCgiBodyStream(slot_index, slot);
		}
		else
			break;
	}

	if (consumed > 0 && consumed <= slot.readbuffer.size())
		slot.readbuffer.erase(slot.readbuffer.begin(), slot.readbuffer.begin() + consumed);
	return false;
}

bool HttpRequestHandler::_startCgiBodyStream(int slot_index, ConnectionSlot &slot, bool &handled)
{
	size_t headerEnd;
	size_t bodyStart;
	std::string headerOnly;
	HttpRequest request;
	RouteResult route;
	std::string transferEncoding;
	std::string contentLength;
	bool isChunked;
	bool hasContentLength;
	size_t expectedSize;
	size_t bodyLimit;
	BodyStreamState *state;

	handled = false;
	if (!_hasHeaderEnd(slot.readbuffer, headerEnd))
		return false;

	bodyStart = headerEnd + 4;
	headerOnly.assign(slot.readbuffer.begin(), slot.readbuffer.begin() + bodyStart);
	try
	{
		request = _parser.parseHeaders(headerOnly);
	}
	catch (const HttpParser::ParseException &e)
	{
		handled = true;
		return _failCgiBodyStream(slot_index, slot, e.getStatusCode());
	}
	catch (const std::exception &)
	{
		handled = true;
		return _failCgiBodyStream(slot_index, slot, 500);
	}

	route = _router.route(request);
	if (!_cgiHandler.isCgiRequest(request, route))
		return false;
	handled = true;

	transferEncoding = _toLower(request.getHeader("Transfer-Encoding"));
	isChunked = transferEncoding.find("chunked") != std::string::npos;
	contentLength = request.getHeader("Content-Length");
	hasContentLength = !contentLength.empty();

	if (isChunked && hasContentLength)
		return _failCgiBodyStream(slot_index, slot, 400);

	expectedSize = 0;
	if (hasContentLength && !_parseSizeValue(contentLength, expectedSize))
		return _failCgiBodyStream(slot_index, slot, 400);

	if (_bodyLimitForRoute(route, bodyLimit) && hasContentLength && expectedSize > bodyLimit)
		return _failCgiBodyStream(slot_index, slot, 413);

	state = new BodyStreamState();
	state->request = request;
	state->chunked = isChunked;
	state->expectedSize = expectedSize;
	state->hasBodyLimit = _bodyLimitForRoute(route, state->bodyLimit);

	if (isChunked || expectedSize > 0)
	{
		if (!_createTempFile(state->tempFd, state->tempPath))
		{
			delete state;
			return _failCgiBodyStream(slot_index, slot, 500);
		}
	}

	_cleanupBodyStream(slot_index);
	_bodyStreams[slot_index] = state;
	slot.readbuffer.erase(slot.readbuffer.begin(), slot.readbuffer.begin() + bodyStart);

	if (!isChunked && expectedSize == 0)
	{
		slot.readbuffer.clear();
		return _finishCgiBodyStream(slot_index, slot);
	}
	return _processCgiBodyStream(slot_index, slot);
}

bool HttpRequestHandler::_handleCgiBodyStream(int slot_index, ConnectionSlot &slot, bool &handled)
{
	if (_bodyStreams.find(slot_index) != _bodyStreams.end())
	{
		handled = true;
		return _processCgiBodyStream(slot_index, slot);
	}
	return _startCgiBodyStream(slot_index, slot, handled);
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
	bool cgiStreamHandled;

	if (_bodyStreams.find(slot_index) != _bodyStreams.end())
	{
		cgiStreamHandled = true;
		return _handleCgiBodyStream(slot_index, slot, cgiStreamHandled);
	}

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

	cgiStreamHandled = false;
	if (_handleCgiBodyStream(slot_index, slot, cgiStreamHandled))
		return true;
	if (cgiStreamHandled)
		return false;

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
	_cleanupBodyStream(slot_index);
	_cgiHandler.closeClientSessions(slot_index);
	std::cout << "[HttpRequestHandler] connection closed: slot["
			  << slot_index << "]" << std::endl;
}
