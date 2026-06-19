#ifndef CGI_HANDLER_HPP
# define CGI_HANDLER_HPP

#include "HttpRequest.hpp"
#include "HttpResponse.hpp"
#include "../Router/Router.hpp"
#include "../Cgi/CgiEnv.hpp"
#include "../Cgi/CgiProcess.hpp"
#include "../Cgi/CgiSession.hpp"
#include "../Network_Server/Pollreactor.hpp"
#include <string>
#include <vector>

class CgiHandler
{
    private:
        std::string _buildScriptPath(const HttpRequest &request, const RouteResult &route) const;

        bool _fileExists(const std::string &path) const;
        bool _isDirectory(const std::string &path) const;

        PollReactor *_reactor;
        std::vector<CgiSession> _sessions;

        CgiSession *_findSessionByFd(int fd);
        bool _writeToCgi(CgiSession &session);
        bool _readFromCgi(CgiSession &session);
        void _finishSession(CgiSession &session);
        void _failSession(CgiSession &session, int statusCode);
        void _cleanupDoneSessions();
        static void _onFdEventStatic(void *ctx, int fd, short revents);

        HttpResponse _parseOutput(const std::string &output) const;
        std::string _trim(const std::string &str) const;
        int _parseStatusCode(const std::string &value) const;
    public:
        CgiHandler();
        void setReactor(PollReactor *reactor);

        bool isCgiRequest(const HttpRequest &request, const RouteResult &route) const;
        bool start(int clientSlot, const HttpRequest &request, const RouteResult &route);
        void onFdEvent(int fd, short revents);
        void closeClientSessions(int clientSlot);
};

#endif
