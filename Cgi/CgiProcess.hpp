#ifndef CGI_PROCESS_HPP
# define CGI_PROCESS_HPP

#include <string>
#include <unistd.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>

class CgiProcess
{
    private:
        pid_t _pid;
        int _stdinFd;
        int _stdoutFd;

        bool _setNonBlocking(int fd) const;
    public:
        CgiProcess();
        
        bool start(const std::string &interpreter, const std::string &scriptPath, char **envp);
        bool startWithInputFile(const std::string &interpreter,
                                const std::string &scriptPath,
                                char **envp,
                                const std::string &inputPath);

        pid_t getPid() const;
        int getStdinFd() const;
        int getStdoutFd() const;
};

#endif
