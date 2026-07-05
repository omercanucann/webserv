#ifndef SIGNALGUARD_HPP
#define SIGNALGUARD_HPP


#include <signal.h>
#include <csignal>
#include <cstdio>
#include <iostream>

class SignalGuard
{
    public:
        static void install();
        static bool shutdown_requested();
    
    private:
        static volatile sig_atomic_t _shutdown_flag;
    
        static void _handler(int signum); 
        SignalGuard();
};

#endif