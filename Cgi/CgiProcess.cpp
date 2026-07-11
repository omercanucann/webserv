#include "CgiProcess.hpp"

CgiProcess::CgiProcess() : _pid(-1), _stdinFd(-1), _stdoutFd(-1)
{

}

bool CgiProcess::_setNonBlocking(int fd) const
{
    if (fcntl(fd, F_SETFL, O_NONBLOCK) == -1)
        return false;
    return true;
}

pid_t CgiProcess::getPid() const
{
    return _pid;
}

int CgiProcess::getStdinFd() const
{
    return _stdinFd;
}

int CgiProcess::getStdoutFd() const
{
    return _stdoutFd;
}

bool CgiProcess::start(const std::string &interpreter, const std::string &scriptPath, char **envp)
{
    int inPipeWC[2];
    int outPipeCW[2];

    if (pipe(inPipeWC) == -1)
        return false;
    
    if (pipe(outPipeCW) == -1)
    {
        close(inPipeWC[0]);
        close(inPipeWC[1]);
        return false;
    }

    _pid = fork();

    if (_pid == -1)
    {
        close(inPipeWC[0]);
        close(inPipeWC[1]);
        close(outPipeCW[0]);
        close(outPipeCW[1]);
        return false;
    }

    if (_pid == 0)
    {
        dup2(inPipeWC[0], STDIN_FILENO);
        dup2(outPipeCW[1], STDOUT_FILENO);

        close(inPipeWC[0]);
        close(inPipeWC[1]);
        close(outPipeCW[0]);
        close(outPipeCW[1]);

        char *argv[3];

        argv[0] = const_cast<char *>(interpreter.c_str());
        argv[1] = const_cast<char *>(scriptPath.c_str());
        argv[2] = NULL;

        execve(interpreter.c_str(), argv, envp);
        exit(1);
    }

    close(inPipeWC[0]);
    close(outPipeCW[1]);

    _stdinFd = inPipeWC[1];
    _stdoutFd = outPipeCW[0];

    _setNonBlocking(_stdinFd);
    _setNonBlocking(_stdoutFd);

    return true;
}

bool CgiProcess::startWithInputFile(const std::string &interpreter,
                                    const std::string &scriptPath,
                                    char **envp,
                                    const std::string &inputPath)
{
    int inputFd;
    int outPipeCW[2];

    inputFd = open(inputPath.c_str(), O_RDONLY);
    if (inputFd < 0)
        return false;

    if (pipe(outPipeCW) == -1)
    {
        close(inputFd);
        return false;
    }

    _pid = fork();
    if (_pid == -1)
    {
        close(inputFd);
        close(outPipeCW[0]);
        close(outPipeCW[1]);
        return false;
    }

    if (_pid == 0)
    {
        dup2(inputFd, STDIN_FILENO);
        dup2(outPipeCW[1], STDOUT_FILENO);

        close(inputFd);
        close(outPipeCW[0]);
        close(outPipeCW[1]);

        char *argv[3];

        argv[0] = const_cast<char *>(interpreter.c_str());
        argv[1] = const_cast<char *>(scriptPath.c_str());
        argv[2] = NULL;

        execve(interpreter.c_str(), argv, envp);
        exit(1);
    }

    close(inputFd);
    close(outPipeCW[1]);

    _stdinFd = -1;
    _stdoutFd = outPipeCW[0];

    _setNonBlocking(_stdoutFd);

    return true;
}
