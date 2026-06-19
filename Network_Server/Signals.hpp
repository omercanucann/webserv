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
        // Tüm sinyalleri kur (SIGINT, SIGTERM, SIGQUIT, SIGABRT, SIGHUP, SIGPIPE, SIGCHLD)
        static void install();

        // Programın kapanması istendi mi?
        static bool shutdown_requested();

        // Konfigürasyon yenilenmesi istendi mi? (SIGHUP)
        static bool reload_requested();

        // Reload flag'ini temizle
        static void clear_reload_flag();

        // Child process sonlandı mı? (CGI işlemleri için)
        static bool child_terminated();

        // Child flag'ini temizle
        static void clear_child_flag();

    private:
        // Shutdown flag - SIGINT, SIGTERM, SIGQUIT, SIGABRT sinyalleri bu flagi set eder
        static volatile sig_atomic_t _shutdown_flag;

        // Reload flag - SIGHUP sinyali bu flagi set eder
        static volatile sig_atomic_t _reload_flag;

        // Child process flag - SIGCHLD sinyali bu flagi set eder
        static volatile sig_atomic_t _child_terminated;

        // Ana sinyal handler (SIGINT, SIGTERM, SIGQUIT, SIGABRT, SIGHUP)
        static void _handler(int signum);

        // Child process handler (SIGCHLD)
        static void _child_handler(int signum);

        // Private constructor - static sınıf olduğu için instantiate edilemez
        SignalHandler();
        ~SignalHandler();
};

#endif // SIGNALS_HPP
