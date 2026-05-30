#ifndef CONNECTIONSLOT_HPP
#define CONNECTIONSLOT_HPP

#include "Nettypes.hpp"
#include <string>
#include <ctime>
#include <cstring>

class ConnectionSlot
{
public:
    ConnectionState state; // Bağlantının mevcut durumu (boş, okuma, yazma, kapatma)
    int             fd; // Bağlantıya atanmış dosya tanıtıcısı, accept() ile atanır, -1 ise geçersiz
    bool            is_server; // Bu slot bir sunucu soketi mi? (true ise, accept() ile yeni bağlantılar bekler, false ise istemci bağlantısı)
 
    std::string     remote_ip;   // İstemci IP adresi ("192.168.1.5")
    uint16_t        remote_port; // İstemci portu
 
    std::vector<char> readbuffer; // Okunan veriler burada birikir, üst katman işleyene kadar
    bool              request_complete; // Üst katman "istek bitti" dedi mi?

    std::vector<char> writebuffer; // Gönderilecek veriler burada birikir, yazma işlemi tamamlanana kadar
    size_t            write_position; // writebuffer içinde şu ana kadar kaç byte yazıldı, yazma işlemi tamamlanana kadar artar
 
    time_t  last_active; // Son okuma/yazma anı — timeout tespiti için
 
    int self_index; // Bağlantı havuzundaki kendi indexi, kapatma işlemi sırasında kolay erişim için
    int origin_server_fd; // accept() çağrısının yapıldığı sunucu fd'si

 
    ConnectionSlot()
        : state(ConnectState_EMPTY) // Başlangıçta slot boş, kullanılmıyor
        , fd(-1) // Geçersiz dosya tanıtıcısı, henüz bir bağlantı atanmadı
        , is_server(false) // Bu slot bir sunucu soketi değil, istemci bağlantısı için kullanılacak
        , remote_port(0) // Geçersiz port numarası, henüz atanmadı
        , request_complete(false) // Henüz bir istek tamamlanmadı
        , write_position(0) // Henüz yazma işlemi başlamadı, pozisyon sıfır
        , last_active(0) // Henüz aktif değil, zaman damgası sıfır
        , self_index(-1) // Henüz havuzda bir index atanmadı, geçersiz index
        , origin_server_fd(-1) // Henüz bir sunucu fd'si atanmadı, geçersiz fd
    {}
 
    void clear()
    {
        state            = ConnectState_EMPTY; // Slot boş, kullanılmıyor
        fd               = -1; // Geçersiz dosya tanıtıcısı
        is_server        = false; // Bu slot bir sunucu soketi değil
        remote_ip.clear(); // İstemci IP adresi temizleniyor
        remote_port      = 0; // Geçersiz port numarası
        self_index         = -1; // Geçersiz index
        readbuffer.clear(); // Okunan veriler temizleniyor
        request_complete = false; // İstek tamamlanmadı
        writebuffer.clear(); // Gönderilecek veriler temizleniyor
        write_position        = 0; // Yazma işlemi başlamadı, pozisyon sıfır
        last_active      = 0; // Aktif değil, zaman damgası sıfır
        origin_server_fd = -1; // Geçersiz sunucu fd'si
    }
 
    bool is_empty()     const { return state == ConnectState_EMPTY; } // Slot boş, kullanılmıyor
    bool is_reading()   const { return state == ConnectState_READING; } // İstemciden veri okuyoruz
    bool is_writing()   const { return state == ConnectState_WRITING; } // İstemciye yanıt yazıyoruz
    bool is_closing()   const { return state == ConnectState_CLOSING; } // Bağlantı kapatma aşamasında
    bool write_done()   const { return write_position >= writebuffer.size(); } // Yazma işlemi tamamlandı mı?
 
    size_t pending_write() const // Yazılacak veri miktarını döndürür
    {
        if (write_position >= writebuffer.size()) return 0; // Yazılacak veri kalmadı
        return writebuffer.size() - write_position; // Yazılacak kalan veri miktarı
    }
 
    bool timed_out() const // Bağlantının zaman aşımına uğrayıp uğramadığını kontrol eder
    {
        if (last_active == 0) return false; // Henüz aktif olmadı, timeout olmaz
        return (time(NULL) - last_active) > CONNECT_IDLE_TIMEOUT; // Son aktiflik zamanından beri geçen süre, timeout süresini aşıyor mu?
    }
 
    void queue_response(const std::vector<char> &data) // Gönderilecek verileri sıraya ekler
    {
        writebuffer	= data; // Gönderilecek veriler yaz bufferına kopyalanıyor
        write_position = 0; // Yazma işlemi henüz başlamadı, pozisyon sıfır
        state	= ConnectState_WRITING; // Slot durumu yazma aşamasına geçiyor, artık bu slot yazma işlemi yapacak
    }
 
    void queue_response(const std::string &data) // Gönderilecek verileri sıraya ekler (string versiyonu)
    {
        writebuffer.assign(data.begin(), data.end()); // String veriler yaz bufferına kopyalanıyor
        write_position = 0; // Yazma işlemi henüz başlamadı, pozisyon sıfır
        state     = ConnectState_WRITING; // Slot durumu yazma aşamasına geçiyor, artık bu slot yazma işlemi yapacak
    }
 
    void touch() { last_active = time(NULL); } // Bağlantının son aktiflik zamanını günceller, timeout kontrolü için kullanılır
};

#endif