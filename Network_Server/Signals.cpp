#include "Signals.hpp"
#include <iostream>
#include <cstring>

volatile sig_atomic_t SignalHandler::_shutdown_flag = 0;
volatile sig_atomic_t SignalHandler::_reload_flag = 0;
volatile sig_atomic_t SignalHandler::_child_terminated = 0;

void SignalHandler::install()
{
    struct sigaction sa;
    struct sigaction oldsa;

    std::memset(&sa, 0, sizeof(sa));

    sa.sa_handler = _handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    if (sigaction(SIGINT, &sa, &oldsa) < 0)
        std::cerr << "[SignalHandler] SIGINT Installation failed.z" << std::endl;

    if (sigaction(SIGTERM, &sa, &oldsa) < 0) 
        std::cerr << "[SignalHandler] SIGTERM Installation failed.z" << std::endl;

    if (sigaction(SIGQUIT, &sa, &oldsa) < 0)
        std::cerr << "[SignalHandler] SIGQUIT Installation failed.z" << std::endl;

    if (sigaction(SIGABRT, &sa, &oldsa) < 0)
        std::cerr << "[SignalHandler] SIGABRT Installation failed.z" << std::endl;

    if (sigaction(SIGHUP, &sa, &oldsa) < 0)
        std::cerr << "[SignalHandler] SIGHUP Installation failed.z" << std::endl;

    std::memset(&sa, 0, sizeof(sa));
    sa.sa_handler = SIG_IGN;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    if (sigaction(SIGPIPE, &sa, &oldsa) < 0)
        std::cerr << "[SignalHandler] SIGPIPE Installation failed.z" << std::endl;

    std::memset(&sa, 0, sizeof(sa));
    sa.sa_handler = _child_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    if (sigaction(SIGCHLD, &sa, &oldsa) < 0)
        std::cerr << "[SignalHandler] SIGCHLD Installation failed.z" << std::endl;

    std::cout << "[SignalHandler] All signals have been established: " << std::endl;
    std::cout << "  - SIGINT (Ctrl+C)" << std::endl;
    std::cout << "  - SIGTERM (kill)" << std::endl;
    std::cout << "  - SIGQUIT (Ctrl+\\)" << std::endl;
    std::cout << "  - SIGABRT (abort)" << std::endl;
    std::cout << "  - SIGHUP (reload)" << std::endl;
    std::cout << "  - SIGPIPE (ignored)" << std::endl;
    std::cout << "  - SIGCHLD (child process)" << std::endl;
}

bool SignalHandler::shutdown_requested()
{
    return _shutdown_flag != 0;
}

bool SignalHandler::reload_requested()
{
    return _reload_flag != 0; 
}

void SignalHandler::clear_reload_flag()
{
    _reload_flag = 0;
}

bool SignalHandler::child_terminated()
{
    return _child_terminated != 0;
}

void SignalHandler::clear_child_flag()
{
    _child_terminated = 0;
}

void SignalHandler::_handler(int signum)
{
    if (signum == SIGINT || signum == SIGTERM || signum == SIGQUIT || signum == SIGABRT)
    {
        std::cerr << "\n[SignalHandler] Signal received: " << signum << " - It's closing...." << std::endl;
        _shutdown_flag = 1;
    }
    else if (signum == SIGHUP)
    {
        std::cerr << "[SignalHandler] SIGHUP received - Configuration reloading..." << std::endl;
        _reload_flag = 1;
    }
}

void SignalHandler::_child_handler(int signum)
{
    (void)signum;
    _child_terminated = 1;
}

SignalHandler::SignalHandler()
{
}

SignalHandler::~SignalHandler()
{
}
