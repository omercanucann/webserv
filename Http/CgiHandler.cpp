#include "CgiHandler.hpp"
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <cerrno>
#include <ctime>
#include <sstream>

CgiHandler::CgiHandler() : _reactor(NULL)
{

}

void CgiHandler::setReactor(PollReactor *reactor)
{
    _reactor = reactor;
}

std::string CgiHandler::_buildScriptPath(const HttpRequest &request, const RouteResult &route) const
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

bool CgiHandler::_fileExists(const std::string &path) const
{
    struct stat st;

    if (stat(path.c_str(), &st) != 0)
        return false;

    return true;
}

bool CgiHandler::_isDirectory(const std::string &path) const
{
    struct stat st;

    if (stat(path.c_str(), &st) != 0)
        return false;

    return S_ISDIR(st.st_mode);
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
    ssize_t n;
    size_t remaining;

    if (session.stdinClosed)
        return true;

    remaining = session.input.size() - session.inputSent;

    if (remaining > 0)
    {
        n = write(session.stdinFd,
                  session.input.c_str() + session.inputSent,
                  remaining);

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
    char buffer[4096];
    ssize_t n;

    n = read(session.stdoutFd, buffer, sizeof(buffer));

    if (n > 0)
    {
        session.output.append(buffer, static_cast<size_t>(n));
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

    waitpid(session.pid, &status, WNOHANG);

    response = _parseOutput(session.output);
    if (_reactor)
        _reactor->queue_response(session.clientSlot, response.toString());

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
        waitpid(session.pid, NULL, WNOHANG);

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

    scriptPath = _buildScriptPath(request, route);

    if (!_fileExists(scriptPath))
    {
        response = HttpResponse::makeErrorResponse(404);
        _reactor->queue_response(clientSlot, response.toString());
        return true;
    }

    if (_isDirectory(scriptPath))
    {
        response = HttpResponse::makeErrorResponse(403);
        _reactor->queue_response(clientSlot, response.toString());
        return true;
    }

    envp = env.build(request, route, scriptPath, "");

    if (!process.start(route.location->cgiInterpreter, scriptPath, envp))
    {
        response = HttpResponse::makeErrorResponse(500);
        _reactor->queue_response(clientSlot, response.toString());
        return true;
    }

    session.clientSlot = clientSlot;
    session.pid = process.getPid();
    session.stdinFd = process.getStdinFd();
    session.stdoutFd = process.getStdoutFd();
    session.input = request.getBody();
    session.inputSent = 0;
    session.startedAt = time(NULL);

    _sessions.push_back(session);
    CgiSession &stored = _sessions.back();

    if (stored.input.empty())
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
                waitpid(_sessions[i].pid, NULL, WNOHANG);
            _sessions[i].done = true;
        }
        i++;
    }
    _cleanupDoneSessions();
}



std::string CgiHandler::_trim(const std::string &str) const
{
    size_t start;
    size_t end;

    start = 0;
    while (start < str.length()
        && (str[start] == ' ' || str[start] == '\t' || str[start] == '\r'))
        start++;

    end = str.length();
    while (end > start
        && (str[end - 1] == ' ' || str[end - 1] == '\t' || str[end - 1] == '\r'))
        end--;

    return str.substr(start, end - start);
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

HttpResponse CgiHandler::_parseOutput(const std::string &output) const
{
    HttpResponse response;
    std::string headerPart;
    std::string body;
    std::string line;
    size_t separator;
    size_t colonPos;

    separator = output.find("\r\n\r\n");
    if (separator != std::string::npos)
    {
        headerPart = output.substr(0, separator);
        body = output.substr(separator + 4);
    }
    else
    {
        separator = output.find("\n\n");
        if (separator != std::string::npos)
        {
            headerPart = output.substr(0, separator);
            body = output.substr(separator + 2);
        }
        else
        {
            response.setStatus(200);
            response.setHeader("Content-Type", "text/plain");
            response.setBody(output);
            return response;
        }
    }

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
        std::string value = _trim(line.substr(colonPos + 1));

        if (key == "Status")
            response.setStatus(_parseStatusCode(value));
        else
            response.setHeader(key, value);
    }

    if (!response.hasHeader("Content-Type"))
        response.setHeader("Content-Type", "text/plain");

    response.setBody(body);
    return response;
}
