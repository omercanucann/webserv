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
        bool _createTempFile(const std::string &prefix, int &fd, std::string &path) const;
        bool _writeAll(int fd, const char *data, size_t size) const;
        void _closeOutputFile(CgiSession &session);
        void _removeOutputFile(CgiSession &session);
        static void _onFdEventStatic(void *ctx, int fd, short revents);

        HttpResponse _parseOutput(const std::string &output) const;
        bool _parseOutputFile(const CgiSession &session, HttpResponse &response,
                              size_t &bodyOffset, size_t &bodyLength) const;
        std::string _trim(const std::string &str) const;
        std::string _toLower(const std::string &str) const;
        std::string _sizeToString(size_t value) const;
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
