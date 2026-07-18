#include "Signals.hpp"
#include <iostream>
#include <cstring>

volatile sig_atomic_t SignalHandler::_shutdown_flag = 0;
volatile sig_atomic_t SignalHandler::_reload_flag = 0;
volatile sig_atomic_t SignalHandler::_child_terminated = 0;

void SignalHandler::install()
{
    if (signal(SIGINT, _handler) == SIG_ERR)
        std::cerr << "[SignalHandler] SIGINT installation failed." << std::endl;

    if (signal(SIGTERM, _handler) == SIG_ERR)
        std::cerr << "[SignalHandler] SIGTERM installation failed." << std::endl;

    if (signal(SIGQUIT, _handler) == SIG_ERR)
        std::cerr << "[SignalHandler] SIGQUIT installation failed." << std::endl;

    if (signal(SIGABRT, _handler) == SIG_ERR)
        std::cerr << "[SignalHandler] SIGABRT installation failed." << std::endl;

    if (signal(SIGHUP, _handler) == SIG_ERR)
        std::cerr << "[SignalHandler] SIGHUP installation failed." << std::endl;

    if (signal(SIGPIPE, SIG_IGN) == SIG_ERR)
        std::cerr << "[SignalHandler] SIGPIPE installation failed." << std::endl;

    if (signal(SIGCHLD, _child_handler) == SIG_ERR)
        std::cerr << "[SignalHandler] SIGCHLD installation failed." << std::endl;

    std::cout << "[SignalHandler] All signals have been established:" << std::endl;
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
