#ifndef CGI_HANDLER_HPP
# define CGI_HANDLER_HPP

#include "HttpRequest.hpp"
#include "HttpResponse.hpp"
#include "../../Router/Router.hpp"
#include "../../Cgi/CgiSession.hpp"
#include <string>
#include <vector>

class PollReactor;

class CgiHandler
{
    private:
        PollReactor *_reactor;
        std::vector<CgiSession> _sessions;

        CgiSession *_findSessionByFd(int fd);
        bool _writeToCgi(CgiSession &session);
        bool _readFromCgi(CgiSession &session);
        void _finishSession(CgiSession &session);
        void _failSession(CgiSession &session, int statusCode);
        void _cleanupDoneSessions();
        void _closeOutputFile(CgiSession &session);
        void _removeOutputFile(CgiSession &session);
        static void _onFdEventStatic(void *ctx, int fd, short revents);

        bool _parseOutputFile(const CgiSession &session, HttpResponse &response,
                              size_t &bodyOffset, size_t &bodyLength) const;
        int _parseStatusCode(const std::string &value) const;
    public:
        CgiHandler();
        void setReactor(PollReactor *reactor);

        bool isCgiRequest(const HttpRequest &request, const RouteResult &route) const;
        bool start(int clientSlot, const HttpRequest &request, const RouteResult &route);
        void onFdEvent(int fd, short revents);
        void closeClientSessions(int clientSlot);
        bool timeoutClientSessions(int clientSlot);
};

#endif
