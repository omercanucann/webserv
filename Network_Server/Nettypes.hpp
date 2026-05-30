#ifndef NETTYPES_HPP
#define NETTYPES_HPP

#include <string>
#include <vector>
#include <stdint.h>
#include <sys/types.h>

static const int MAX_SLOTS         = 1024; // Maksimum bağlantı slot sayısı
static const int READ_CHUNK        = 16384; // Bir seferde okunacak maksimum veri miktarı (16 KB)
static const int POLL_TIMEOUT_MS   = 5000; // poll() fonksiyonunun zaman aşımı süresi (ms cinsinden)
static const int CONNECT_IDLE_TIMEOUT = 60; // Bağlantının boşta kalma süresi (saniye cinsinden), bu süre sonunda bağlantı kapatılabilir
static const int MAX_SERVERS       = 8; // Maksimum sunucu sayısı, bu kadar farklı IP:port kombinasyonunu dinleyebiliriz


enum ConnectionState
{
    ConnectState_EMPTY   = 0,   // Slot boş, kullanılmıyor
    ConnectState_READING = 1,   // İstemciden veri okuyoruz
    ConnectState_WRITING = 2,   // İstemciye yanıt yazıyoruz
    ConnectState_CLOSING = 3    // Bağlantı kapatma aşamasında
};


struct ServerBind
{
    std::string host;      // "127.0.0.1" veya "0.0.0.0"
    uint16_t    port;      // 1-65535
    int         fd;        // bind() + listen() sonrası açık fd (-1 = henüz açılmadı)
 
    ServerBind() : port(0), fd(-1) {} // Varsayılan constructor
    ServerBind(const std::string &hst, uint16_t prt): host(hst), port(prt), fd(-1) {} // Parametreli constructor
};

inline bool fd_valid(int fd)
{
    return fd > 0; 
}

#endif