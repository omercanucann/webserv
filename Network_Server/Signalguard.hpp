#ifndef SIGNALGUARD_HPP
#define SIGNALGUARD_HPP


#include <signal.h>
#include <csignal>
#include <cstdio>
#include <iostream>

class SignalGuard
{
    public:
        static void install(); // ne işe yarar?: Sinyal işleyicilerini kurar ve sinyal yakalama mekanizmasını başlatır.
        static bool shutdown_requested(); // ne işe yarar?: Programın kapanması istenip istenmediğini kontrol eder. Eğer kapanma istenmişse true döner, aksi halde false döner.
    
    private:
        static volatile sig_atomic_t _shutdown_flag; // ne işe yarar?: Sinyal işleyicisi tarafından değiştirilebilen ve programın kapanma isteğini göstermek için kullanılan bir bayrak. 
        //volatile anahtar kelimesi, bu değişkenin sinyal işleyicisi tarafından değiştirilebileceğini belirtir ve sig_atomic_t türü, bu değişkenin atomik olarak erişilebilir olduğunu garanti eder. 
    
        static void _handler(int signum); // ne işe yarar?: Sinyal işleyicisi olarak kullanılan bir fonksiyon. Bu fonksiyon, belirli bir sinyal alındığında çağrılır ve 
        //shutdown_flag'ı true yaparak programın kapanma isteğini belirtir. signum parametresi, alınan sinyalin numarasını temsil eder.   
        SignalGuard(); // ne işe yarar?: Sinyal koruyucu sınıfının yapıcı fonksiyonu. Bu sınıfın örneklerinin oluşturulmasını engellemek için özel olarak tanımlanmıştır.
};

#endif