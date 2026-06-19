#include "Signals.hpp"
#include <iostream>
#include <cstring>

// Global static member initialization
volatile sig_atomic_t SignalHandler::_shutdown_flag = 0;
volatile sig_atomic_t SignalHandler::_reload_flag = 0;
volatile sig_atomic_t SignalHandler::_child_terminated = 0;

void SignalHandler::install()
{
    struct sigaction sa;
    struct sigaction oldsa;

    // sigaction yapısını sıfırla
    std::memset(&sa, 0, sizeof(sa));

    // Signal handler fonksiyonunu ata
    sa.sa_handler = _handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    // SIGINT - Terminal kesintisi (Ctrl+C)
    if (sigaction(SIGINT, &sa, &oldsa) < 0)
        std::cerr << "[SignalHandler] SIGINT kurulumu başarısız" << std::endl;

    // SIGTERM - Programı sonlandırma sinyali
    if (sigaction(SIGTERM, &sa, &oldsa) < 0)
        std::cerr << "[SignalHandler] SIGTERM kurulumu başarısız" << std::endl;

    // SIGQUIT - Quit sinyali (Ctrl+\)
    if (sigaction(SIGQUIT, &sa, &oldsa) < 0)
        std::cerr << "[SignalHandler] SIGQUIT kurulumu başarısız" << std::endl;

    // SIGABRT - Programı sonlandırma sinyali (abort())
    if (sigaction(SIGABRT, &sa, &oldsa) < 0)
        std::cerr << "[SignalHandler] SIGABRT kurulumu başarısız" << std::endl;

    // SIGHUP - Bağlantı kopması / Konfigürasyon yenileme
    if (sigaction(SIGHUP, &sa, &oldsa) < 0)
        std::cerr << "[SignalHandler] SIGHUP kurulumu başarısız" << std::endl;

    // SIGPIPE - Broken pipe (yazma işlemi başarısız olduğunda)
    // Bu sinyali yoksayıyoruz, çünkü write() -1 döndürsün
    std::memset(&sa, 0, sizeof(sa));
    sa.sa_handler = SIG_IGN;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    if (sigaction(SIGPIPE, &sa, &oldsa) < 0)
        std::cerr << "[SignalHandler] SIGPIPE kurulumu başarısız" << std::endl;

    // SIGCHLD - Child process sonlandı (CGI işlemleri için)
    std::memset(&sa, 0, sizeof(sa));
    sa.sa_handler = _child_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    if (sigaction(SIGCHLD, &sa, &oldsa) < 0)
        std::cerr << "[SignalHandler] SIGCHLD kurulumu başarısız" << std::endl;

    std::cout << "[SignalHandler] Tüm sinyaller kuruldu:" << std::endl;
    std::cout << "  - SIGINT (Ctrl+C)" << std::endl;
    std::cout << "  - SIGTERM (kill)" << std::endl;
    std::cout << "  - SIGQUIT (Ctrl+\\)" << std::endl;
    std::cout << "  - SIGABRT (abort)" << std::endl;
    std::cout << "  - SIGHUP (reload)" << std::endl;
    std::cout << "  - SIGPIPE (yoksayıldı)" << std::endl;
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
        std::cerr << "\n[SignalHandler] Signal alındı: " << signum << " - Kapanıyor..." << std::endl;
        _shutdown_flag = 1;
    }
    else if (signum == SIGHUP)
    {
        std::cerr << "[SignalHandler] SIGHUP alındı - Konfigürasyon yenileniyor..." << std::endl;
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
    // Private constructor - static üyeler için kullanılmaz
}

SignalHandler::~SignalHandler()
{
    // Destructor - hiçbir şey yapmaz
}
