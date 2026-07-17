#include "HttpRequestHandler.hpp"
#include "HttpFraming.hpp"
#include <iostream>

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
	: _parser(_getParserLimit(config)), _router(config), _staticHandler(),
	  _cgiHandler(), _reactor(&reactor)
{
	_cgiHandler.setReactor(&reactor);
}

bool HttpRequestHandler::handle_data(int slot_index, ConnectionSlot &slot)
{
	std::string rawRequest;
	HttpRequest request;
	HttpResponse response;
	bool cgiStreamHandled;

	if (_bodyStreams.find(slot_index) != _bodyStreams.end())
	{
		cgiStreamHandled = true;
		return _handleCgiBodyStream(slot_index, slot, cgiStreamHandled);
	}

	HttpFraming::reserveForContentLength(slot.readbuffer);
	_sendContinueIfNeeded(slot);

	if (_makePreflightResponse(slot.readbuffer, response))
	{
		_storeFinalResponse(slot, response);
		return true;
	}

	cgiStreamHandled = false;
	if (_handleCgiBodyStream(slot_index, slot, cgiStreamHandled))
		return true;
	if (cgiStreamHandled)
		return false;

	if (!HttpFraming::isRequestComplete(slot.readbuffer))
		return false;

	rawRequest.assign(slot.readbuffer.begin(), slot.readbuffer.end());

	try
	{
		request = _parser.parse(rawRequest);
		if (!_dispatchRequest(slot_index, request, response))
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

	_storeFinalResponse(slot, response);

	return true;
}

void HttpRequestHandler::handle_close(int slot_index)
{
	_cleanupBodyStream(slot_index);
	_cgiHandler.closeClientSessions(slot_index);
	std::cout << "[HttpRequestHandler] connection closed: slot["
				<< slot_index << "]" << std::endl;
}

bool HttpRequestHandler::handle_timeout(int slot_index, ConnectionSlot &slot)
{
	(void)slot;
	return _cgiHandler.timeoutClientSessions(slot_index);
}
