#ifndef CGI_SESSION_HPP
# define CGI_SESSION_HPP

#include <string>
#include <ctime>
#include <sys/types.h>

struct CgiSession
{
    int clientSlot;
    pid_t pid;

    int stdinFd;
    int stdoutFd;

    std::string input;
    size_t inputSent;

    std::string output;
    int outputFileFd;
    std::string outputFilePath;
    size_t outputSize;

    bool stdinClosed;
    bool stdoutClosed;
    bool done;

    time_t startedAt;

    CgiSession()
        : clientSlot(-1),
          pid(-1),
          stdinFd(-1),
          stdoutFd(-1),
          inputSent(0),
          outputFileFd(-1),
          outputSize(0),
          stdinClosed(false),
          stdoutClosed(false),
          done(false),
          startedAt(0)
    {}
};

#endif
