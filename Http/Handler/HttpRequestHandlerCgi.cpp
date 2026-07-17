#include "HttpRequestHandler.hpp"
#include "HttpFraming.hpp"
#include "../../utils/FileUtils.hpp"
#include "../../utils/StringUtils.hpp"
#include <cstdio>
#include <unistd.h>

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

	_cleanupBodyStream(slot_index);
	slot.readbuffer.clear();
	response = _makeErrorResponse(statusCode, NULL);
	_storeFinalResponse(slot, response);
	return true;
}

bool HttpRequestHandler::_finishCgiBodyStream(int slot_index, ConnectionSlot &slot)
{
	std::map<int, BodyStreamState *>::iterator it;
	BodyStreamState *state;
	HttpResponse response;

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
		if (!_dispatchRequest(slot_index, state->request, response))
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

	_storeFinalResponse(slot, response);
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
			if (!FileUtils::writeAll(state->tempFd, &slot.readbuffer[0], toWrite))
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
				if (!FileUtils::writeAll(state->tempFd, &slot.readbuffer[consumed], toWrite))
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

bool HttpRequestHandler::_startCgiBodyStream(int slot_index,
											ConnectionSlot &slot,
											bool &handled)
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
	if (!HttpFraming::findHeaderEnd(slot.readbuffer, headerEnd))
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

	transferEncoding = StringUtils::toLowerAscii(
		request.getHeader("Transfer-Encoding"));
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
		if (!FileUtils::createTempFile("cgi_body", state->tempFd, state->tempPath))
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

bool HttpRequestHandler::_handleCgiBodyStream(int slot_index,
											 ConnectionSlot &slot,
											 bool &handled)
{
	if (_bodyStreams.find(slot_index) != _bodyStreams.end())
	{
		handled = true;
		return _processCgiBodyStream(slot_index, slot);
	}
	return _startCgiBodyStream(slot_index, slot, handled);
}
