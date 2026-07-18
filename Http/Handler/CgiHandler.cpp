#include "CgiHandler.hpp"
#include "StatusCode.hpp"
#include "RoutePath.hpp"
#include "../../Cgi/CgiEnv.hpp"
#include "../../Cgi/CgiProcess.hpp"
#include "../../Network_Server/Pollreactor.hpp"
#include "../../utils/FileUtils.hpp"
#include "../../utils/StringUtils.hpp"
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctime>
#include <sstream>
#include <cstdio>
#include <signal.h>

CgiHandler::CgiHandler() : _reactor(NULL)
{

}

void CgiHandler::setReactor(PollReactor *reactor)
{
    _reactor = reactor;
}

void CgiHandler::_closeOutputFile(CgiSession &session)
{
    if (session.outputFileFd >= 0)
    {
        close(session.outputFileFd);
        session.outputFileFd = -1;
    }
}

void CgiHandler::_removeOutputFile(CgiSession &session)
{
    _closeOutputFile(session);
    if (!session.outputFilePath.empty())
    {
        std::remove(session.outputFilePath.c_str());
        session.outputFilePath.clear();
    }
    session.outputSize = 0;
}

CgiSession *CgiHandler::_findSessionByFd(int fd)
{
    size_t i;

    i = 0;
    while (i < _sessions.size())
    {
        if (!_sessions[i].done
            && (_sessions[i].stdinFd == fd || _sessions[i].stdoutFd == fd))
            return &_sessions[i];
        i++;
    }
    return NULL;
}

bool CgiHandler::_writeToCgi(CgiSession &session)
{
    static const size_t CGI_IO_CHUNK = 262144;
    ssize_t n;
    size_t remaining;
    size_t writeSize;

    if (session.stdinClosed)
        return true;

    remaining = session.input.size() - session.inputSent;

    if (remaining > 0)
    {
        writeSize = remaining;
        if (writeSize > CGI_IO_CHUNK)
            writeSize = CGI_IO_CHUNK;
        n = write(session.stdinFd,
                  session.input.c_str() + session.inputSent,
                  writeSize);

        if (n > 0)
            session.inputSent += static_cast<size_t>(n);
        else
            return false;
    }

    if (session.inputSent >= session.input.size())
    {
        if (_reactor)
            _reactor->unwatch_fd(session.stdinFd);
        close(session.stdinFd);
        session.stdinFd = -1;
        session.stdinClosed = true;
    }

    return true;
}

bool CgiHandler::_readFromCgi(CgiSession &session)
{
    char buffer[262144];
    ssize_t n;

    n = read(session.stdoutFd, buffer, sizeof(buffer));

    if (n > 0)
    {
        if (session.outputFileFd < 0)
            return false;
        if (!FileUtils::writeAll(session.outputFileFd, buffer, static_cast<size_t>(n)))
            return false;
        session.outputSize += static_cast<size_t>(n);
        return true;
    }

    if (n == 0)
    {
        _finishSession(session);
        return true;
    }

    return false;
}

bool CgiHandler::isCgiRequest(const HttpRequest &request, const RouteResult &route) const
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

void CgiHandler::_finishSession(CgiSession &session)
{
    HttpResponse response;
    int status;
    size_t bodyOffset;
    size_t bodyLength;
    std::string rawHeader;

    if (session.done)
        return;

    if (!session.stdinClosed && session.stdinFd >= 0)
    {
        if (_reactor)
            _reactor->unwatch_fd(session.stdinFd);
        close(session.stdinFd);
        session.stdinFd = -1;
        session.stdinClosed = true;
    }

    if (!session.stdoutClosed && session.stdoutFd >= 0)
    {
        if (_reactor)
            _reactor->unwatch_fd(session.stdoutFd);
        close(session.stdoutFd);
        session.stdoutFd = -1;
        session.stdoutClosed = true;
    }

    if (session.pid <= 0
        || waitpid(session.pid, &status, 0) != session.pid
        || !WIFEXITED(status)
        || WEXITSTATUS(status) != 0)
    {
        _removeOutputFile(session);
        response = HttpResponse::makeErrorResponse(500);
        if (_reactor)
            _reactor->queue_response(session.clientSlot, response.toString());
        session.done = true;
        return;
    }

    _closeOutputFile(session);
    if (!_parseOutputFile(session, response, bodyOffset, bodyLength))
    {
        _removeOutputFile(session);
        response = HttpResponse::makeErrorResponse(500);
        if (_reactor)
            _reactor->queue_response(session.clientSlot, response.toString());
        session.done = true;
        return;
    }

    if (_reactor)
    {
        rawHeader = response.toString();
        if (bodyLength > 0)
        {
            if (_reactor->queue_response_file(session.clientSlot, rawHeader,
                    session.outputFilePath, bodyOffset, bodyLength, true))
            {
                session.outputFilePath.clear();
                session.outputSize = 0;
            }
            else
            {
                _removeOutputFile(session);
                response = HttpResponse::makeErrorResponse(500);
                _reactor->queue_response(session.clientSlot, response.toString());
            }
        }
        else
        {
            _removeOutputFile(session);
            _reactor->queue_response(session.clientSlot, rawHeader);
        }
    }
    else
        _removeOutputFile(session);

    session.done = true;
}

void CgiHandler::_failSession(CgiSession &session, int statusCode)
{
    HttpResponse response;

    if (session.done)
        return;

    if (!session.stdinClosed && session.stdinFd >= 0)
    {
        if (_reactor)
            _reactor->unwatch_fd(session.stdinFd);
        close(session.stdinFd);
        session.stdinFd = -1;
        session.stdinClosed = true;
    }

    if (!session.stdoutClosed && session.stdoutFd >= 0)
    {
        if (_reactor)
            _reactor->unwatch_fd(session.stdoutFd);
        close(session.stdoutFd);
        session.stdoutFd = -1;
        session.stdoutClosed = true;
    }

    if (session.pid > 0)
    {
        kill(session.pid, SIGKILL);
        waitpid(session.pid, NULL, 0);
    }

    _removeOutputFile(session);

    response = HttpResponse::makeErrorResponse(statusCode);
    if (_reactor)
        _reactor->queue_response(session.clientSlot, response.toString());

    session.done = true;
}

void CgiHandler::_cleanupDoneSessions()
{
    std::vector<CgiSession>::iterator it;

    it = _sessions.begin();
    while (it != _sessions.end())
    {
        if (it->done)
            it = _sessions.erase(it);
        else
            ++it;
    }
}

void CgiHandler::_onFdEventStatic(void *ctx, int fd, short revents)
{
    CgiHandler *handler;

    handler = static_cast<CgiHandler *>(ctx);
    if (handler)
        handler->onFdEvent(fd, revents);
}

void CgiHandler::onFdEvent(int fd, short revents)
{
    CgiSession *session;

    session = _findSessionByFd(fd);
    if (session == NULL)
        return;

    if ((revents & (POLLERR | POLLNVAL)) != 0)
        _failSession(*session, 500);
    else
    {
        if ((revents & POLLOUT) != 0 && fd == session->stdinFd)
        {
            if (!_writeToCgi(*session))
                _failSession(*session, 500);
        }
        if (!session->done
            && ((revents & POLLIN) != 0 || (revents & POLLHUP) != 0)
            && fd == session->stdoutFd)
        {
            if (!_readFromCgi(*session))
                _failSession(*session, 500);
        }
    }

    _cleanupDoneSessions();
}

bool CgiHandler::start(int clientSlot, const HttpRequest &request, const RouteResult &route)
{
    CgiEnv env;
    CgiProcess process;
    CgiSession session;
    std::string scriptPath;
    char **envp;
    HttpResponse response;
    int outputFd;
    std::string outputPath;

    if (_reactor == NULL)
        return false;

    if (request.getMethod() != "GET" && request.getMethod() != "POST")
    {
        response = HttpResponse::makeErrorResponse(405);
        _reactor->queue_response(clientSlot, response.toString());
        return true;
    }

    if (route.location == NULL || route.location->cgiInterpreter.empty())
    {
        response = HttpResponse::makeErrorResponse(500);
        _reactor->queue_response(clientSlot, response.toString());
        return true;
    }

    scriptPath = RoutePath::resolve(request, route);

    if (!FileUtils::exists(scriptPath))
    {
        response = HttpResponse::makeErrorResponse(404);
        _reactor->queue_response(clientSlot, response.toString());
        return true;
    }

    if (FileUtils::isDirectory(scriptPath))
    {
        response = HttpResponse::makeErrorResponse(403);
        _reactor->queue_response(clientSlot, response.toString());
        return true;
    }

    envp = env.build(request, route, scriptPath, request.getPath());

    outputFd = -1;
    if (!FileUtils::createTempFile("cgi_out", outputFd, outputPath))
    {
        response = HttpResponse::makeErrorResponse(500);
        _reactor->queue_response(clientSlot, response.toString());
        return true;
    }

    if (request.hasBodyFile())
    {
        if (!process.startWithInputFile(route.location->cgiInterpreter,
                scriptPath, envp, request.getBodyFilePath()))
        {
            close(outputFd);
            std::remove(outputPath.c_str());
            response = HttpResponse::makeErrorResponse(500);
            _reactor->queue_response(clientSlot, response.toString());
            return true;
        }
    }
    else if (!process.start(route.location->cgiInterpreter, scriptPath, envp))
    {
        close(outputFd);
        std::remove(outputPath.c_str());
        response = HttpResponse::makeErrorResponse(500);
        _reactor->queue_response(clientSlot, response.toString());
        return true;
    }

    session.clientSlot = clientSlot;
    session.pid = process.getPid();
    session.stdinFd = process.getStdinFd();
    session.stdoutFd = process.getStdoutFd();
    if (!request.hasBodyFile())
        session.input = request.getBody();
    session.inputSent = 0;
    session.outputFileFd = outputFd;
    session.outputFilePath = outputPath;
    session.outputSize = 0;
    session.startedAt = std::time(NULL);

    _sessions.push_back(session);
    CgiSession &stored = _sessions.back();

    if (stored.stdinFd < 0)
        stored.stdinClosed = true;
    else if (stored.input.empty())
    {
        close(stored.stdinFd);
        stored.stdinFd = -1;
        stored.stdinClosed = true;
    }
    else if (!_reactor->watch_fd(stored.stdinFd, POLLOUT, CgiHandler::_onFdEventStatic, this))
    {
        _failSession(stored, 500);
        _cleanupDoneSessions();
        return true;
    }

    if (!_reactor->watch_fd(stored.stdoutFd, POLLIN, CgiHandler::_onFdEventStatic, this))
    {
        _failSession(stored, 500);
        _cleanupDoneSessions();
        return true;
    }

    return true;
}

void CgiHandler::closeClientSessions(int clientSlot)
{
    size_t i;

    i = 0;
    while (i < _sessions.size())
    {
        if (_sessions[i].clientSlot == clientSlot && !_sessions[i].done)
        {
            if (!_sessions[i].stdinClosed && _sessions[i].stdinFd >= 0)
            {
                if (_reactor)
                    _reactor->unwatch_fd(_sessions[i].stdinFd);
                close(_sessions[i].stdinFd);
                _sessions[i].stdinFd = -1;
                _sessions[i].stdinClosed = true;
            }
            if (!_sessions[i].stdoutClosed && _sessions[i].stdoutFd >= 0)
            {
                if (_reactor)
                    _reactor->unwatch_fd(_sessions[i].stdoutFd);
                close(_sessions[i].stdoutFd);
                _sessions[i].stdoutFd = -1;
                _sessions[i].stdoutClosed = true;
            }
            if (_sessions[i].pid > 0)
            {
                kill(_sessions[i].pid, SIGKILL);
                waitpid(_sessions[i].pid, NULL, 0);
            }
            _removeOutputFile(_sessions[i]);
            _sessions[i].done = true;
        }
        i++;
    }
    _cleanupDoneSessions();
}

bool CgiHandler::timeoutClientSessions(int clientSlot)
{
    size_t i;
    bool found;

    found = false;
    i = 0;
    while (i < _sessions.size())
    {
        if (_sessions[i].clientSlot == clientSlot && !_sessions[i].done)
        {
            _failSession(_sessions[i], 504);
            found = true;
        }
        i++;
    }
    _cleanupDoneSessions();
    return found;
}



int CgiHandler::_parseStatusCode(const std::string &value) const
{
    int code;

    code = 0;
    if (value.length() < 3)
        return 200;

    if (value[0] < '0' || value[0] > '9'
        || value[1] < '0' || value[1] > '9'
        || value[2] < '0' || value[2] > '9')
        return 200;

    code = (value[0] - '0') * 100;
    code += (value[1] - '0') * 10;
    code += value[2] - '0';

    if (code < 100 || code > 599)
        return 200;

    return code;
}

bool CgiHandler::_parseOutputFile(const CgiSession &session, HttpResponse &response,
                                  size_t &bodyOffset, size_t &bodyLength) const
{
    static const size_t HEADER_SCAN_LIMIT = 64 * 1024;
    int fd;
    char buffer[8192];
    std::string prefix;
    size_t scanLimit;
    ssize_t n;
    size_t separator;
    size_t separatorLength;
    std::string headerPart;
    std::string line;
    size_t colonPos;

    response = HttpResponse();
    bodyOffset = 0;
    bodyLength = session.outputSize;

    if (session.outputFilePath.empty())
        return false;

    fd = open(session.outputFilePath.c_str(), O_RDONLY);
    if (fd < 0)
        return false;

    scanLimit = session.outputSize;
    if (scanLimit > HEADER_SCAN_LIMIT)
        scanLimit = HEADER_SCAN_LIMIT;

    while (prefix.size() < scanLimit)
    {
        size_t want;

        want = scanLimit - prefix.size();
        if (want > sizeof(buffer))
            want = sizeof(buffer);

        n = read(fd, buffer, want);
        if (n > 0)
            prefix.append(buffer, static_cast<size_t>(n));
        else
            break;
    }
    close(fd);

    separator = prefix.find("\r\n\r\n");
    separatorLength = 4;
    if (separator == std::string::npos)
    {
        separator = prefix.find("\n\n");
        separatorLength = 2;
    }

    if (separator == std::string::npos)
    {
        response.setStatus(200);
        response.setHeader("Content-Type", "text/plain");
        bodyOffset = 0;
        bodyLength = session.outputSize;
        response.setHeader("Content-Length",
            StringUtils::sizeToString(bodyLength));
        return true;
    }

    headerPart = prefix.substr(0, separator);
    bodyOffset = separator + separatorLength;
    if (bodyOffset > session.outputSize)
        bodyLength = 0;
    else
        bodyLength = session.outputSize - bodyOffset;

    response.setStatus(200);

    std::istringstream stream(headerPart);
    while (std::getline(stream, line))
    {
        if (!line.empty() && line[line.length() - 1] == '\r')
            line.erase(line.length() - 1);

        colonPos = line.find(':');
        if (colonPos == std::string::npos)
            continue;

        std::string key = line.substr(0, colonPos);
        std::string value = StringUtils::trim(
            line.substr(colonPos + 1), " \t\r");

        if (key == "Status")
            response.setStatus(_parseStatusCode(value));
        else if (StringUtils::toLowerAscii(key) != "content-length")
            response.setHeader(key, value);
    }

    if (!response.hasHeader("Content-Type"))
        response.setHeader("Content-Type", "text/plain");

    if (!StatusCode::isBodyAllowed(response.getStatusCode()))
        bodyLength = 0;
    response.setHeader("Content-Length", StringUtils::sizeToString(bodyLength));
    return true;
}
