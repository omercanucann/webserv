#ifndef SIGNALS_HPP
#define SIGNALS_HPP

#include <signal.h>
#include <csignal>
#include <cstdio>
#include <iostream>
#include <cstring>

class SignalHandler
{
    public:
        static void install();

        static bool shutdown_requested();

        static bool reload_requested();

        static void clear_reload_flag();

        static bool child_terminated();

        static void clear_child_flag();

    private:
        static volatile sig_atomic_t _shutdown_flag;

        static volatile sig_atomic_t _reload_flag;

        static volatile sig_atomic_t _child_terminated;

        static void _handler(int signum);

        static void _child_handler(int signum);

        SignalHandler();
        ~SignalHandler();
};

#endif
