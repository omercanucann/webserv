#include "Signalguard.hpp"

volatile sig_atomic_t SignalGuard::_shutdown_flag = 0;
 
void SignalGuard::install() // Sinyal handler'ları kur
{
    signal(SIGINT,  _handler); // SIGINT sinyalini yakala (Ctrl+C)
    signal(SIGTERM, _handler); // SIGTERM sinyalini yakala (kill komutu)
    signal(SIGPIPE, SIG_IGN);  // SIGPIPE'ı yoksay, write() -1 dönsün
 
    std::cout << "[SignalGuard] Signal handlers were installed." << std::endl;
}
 
bool SignalGuard::shutdown_requested() // Programın kapanma isteği olup olmadığını kontrol et
{
    return _shutdown_flag != 0; // Eğer shutdown_flag 0 değilse, kapanma isteği var demektir
}
 
void SignalGuard::_handler(int signum) // Sinyal handler fonksiyonu
{
    _shutdown_flag = 1; // Sinyal alındığında shutdown_flag'ı 1 yaparak kapanma isteğini belirtir
    (void)signum; // signum parametresi kullanılmaz, bu satır sadece derleyici uyarısını önlemek içindir
}