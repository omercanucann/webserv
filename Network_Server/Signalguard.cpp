#include "Signalguard.hpp"

volatile sig_atomic_t SignalGuard::_shutdown_flag = 0;
 
void SignalGuard::install()
{
    signal(SIGINT,  _handler);
    signal(SIGTERM, _handler);
    signal(SIGPIPE, SIG_IGN);
 
    std::cout << "[SignalGuard] Signal handlers were installed." << std::endl;
}
 
bool SignalGuard::shutdown_requested()
{
    return _shutdown_flag != 0;
}
 
void SignalGuard::_handler(int signum)
{
    _shutdown_flag = 1;
    (void)signum;
}