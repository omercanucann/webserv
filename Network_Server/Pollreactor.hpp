#ifndef POLLREACTOR_HPP
#define POLLREACTOR_HPP

#include "Nettypes.hpp"
#include "Connectionslot.hpp"
#include "Socketbinder.hpp"
#include <poll.h>
#include <vector>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#include <cstdio>
#include <ctime>
#include <iostream>
#include "../utils/Utils.hpp"

typedef void (*DataCallback)(void *ctx, ConnectionSlot &slot);
typedef void (*FdCallback)(void *ctx, int fd, short revents);

class PollReactor
{
    public:
    
        PollReactor();
        ~PollReactor();
        bool add_server(const std::string &host, uint16_t port);

        void set_callbacks
        (
            DataCallback on_data,    // Yeni veri okunduğunda
            DataCallback on_write,   // Slot yazılmaya hazır olduğunda
            DataCallback on_close,   // Bağlantı kapandığında
            void*        context     // Callback'lere geçirilecek context
        );

        void run(); // Poll döngüsünü başlatır, bu fonksiyon bloklayıcıdır
        void request_shutdown(); // Poll döngüsünü güvenli bir şekilde sonlandırmak için çağrılır
        void queue_response(int slot_index, const std::string &data); // string versiyonu, vector<char> versiyonunu çağırır
        void queue_response(int slot_index, const std::vector<char> &data); // Belirli bir slot için gönderilecek veriyi sıraya ekler, bu veri yazma işlemi sırasında gönderilecektir
        void close_slot(int slot_index);     // Belirli bir slotu kapatır, bu genellikle istemci bağlantısı için kullanılır, sunucu soketleri için kullanılmaz
        int  active_connections() const; // Şu anda aktif olan istemci bağlantılarının sayısını döndürür, sunucu soketleri sayılmaz
        bool is_server_fd(int fd) const; // Verilen dosya tanıtıcısının bir sunucu soketi olup olmadığını kontrol eder, true ise bu fd'ye gelen bağlantılar accept() ile yeni slotlara atanır
    
        bool watch_fd(int fd, short events, FdCallback callback, void *ctx);
        void update_fd(int fd, short events);
        void unwatch_fd(int fd);

    private:
        void _accept_client(int server_slot_index); // Belirli bir sunucu slotuna gelen yeni bağlantıyı kabul eder, bu genellikle accept() çağrısı yapar ve yeni bir istemci slotu oluşturur
        void _read_slot(int slot_index); // Belirli bir slotta veri okumaya çalışır, bu genellikle recv() çağrısı yapar, okunan verileri slotun readbuffer'ına ekler ve üst katmana iletmek için on_data callback'ini çağırır
        void _write_slot(int slot_index); // Belirli bir slotta veri yazmaya çalışır, bu genellikle send() çağrısı yapar, writebuffer'daki verileri gönderir, gönderilen byte kadar write_position'ı artırır, eğer tüm veri gönderildiyse on_write callback'ini çağırır
        void _close_slot(int slot_index); // Belirli bir slotu kapatır, bu genellikle close() çağrısı yapar, slotu temizler ve on_close callback'ini çağırır
        int  _find_empty_slot() const; // Bağlantı havuzunda boş bir slot bulur, eğer boş slot yoksa -1 döndürür
        void _sweep_timeouts(); // Tüm slotları tarar, boşta kalma süresi CONNECT_IDLE_TIMEOUT'u aşan istemci bağlantılarını kapatır, bu fonksiyon düzenli aralıklarla çağrılır (örneğin her 100 poll() turunda bir)
        void _sync_poll_events(int slot_index); // Belirli bir slotun durumuna göre pollfd.events alanını günceller, örneğin okuma durumunda POLLIN, yazma durumunda POLLOUT, kapatma durumunda hiçbir olay beklenmez
        
        ConnectionSlot _slots[MAX_SLOTS]; // Bağlantı havuzu, her slot bir istemci bağlantısını veya sunucu soketini temsil eder, toplam MAX_SLOTS adet olabilir
        struct pollfd  _pollfds[MAX_SLOTS]; // pollfd dizisi, her slotun fd'si ve beklenen olayları içerir, bu dizi poll() fonksiyonuna verilir, slot indexi ile pollfd indexi birebir eşleşir
        int            _slot_count;   // Aktif slot sayısı (boş + dolu)
        ServerBind     _servers[MAX_SERVERS]; // Dinlenen sunucu soketleri, her biri bir IP:port kombinasyonunu temsil eder, toplam MAX_SERVERS adet olabilir
        int            _server_count; // Aktif sunucu sayısı
        DataCallback   _on_data; // Yeni veri okunduğunda çağrılacak callback, bu callback'e context ve ilgili slot iletilir, üst katman bu veriyi işleyebilir ve gerekirse yanıt oluşturabilir
        DataCallback   _on_write; // Slot yazılmaya hazır olduğunda çağrılacak callback, bu callback'e context ve ilgili slot iletilir, üst katman bu slotun yazma işlemi tamamlandığını anlayabilir ve gerekirse yeni veri sıraya ekleyebilir
        DataCallback   _on_close; // Bağlantı kapandığında çağrılacak callback, bu callback'e context ve ilgili slot iletilir, üst katman bu bağlantının kapandığını anlayabilir ve gerekirse kaynakları temizleyebilir
        void*          _cb_ctx; // Callback'lere geçirilecek genel context, bu genellikle üst katmanın kendi durumunu tutmak için kullanılır, örneğin bir HTTP sunucusu için bu context içinde yönlendirme tabloları veya diğer sunucu durum bilgileri olabilir
        bool           _running; // Poll döngüsünün çalışıp çalışmadığını gösteren flag, run() fonksiyonu içinde kontrol edilir, request_shutdown() çağrıldığında bu flag false yapılır ve run() fonksiyonu güvenli bir şekilde sonlanır
        int            _sweep_counter; // Tarama sayacı, her poll() turunda artırılır, SWEEP_INTERVAL'a ulaştığında _sweep_timeouts() fonksiyonu çağrılır ve sayaç sıfırlanır
        static const int SWEEP_INTERVAL = 100; // Her 100 poll() turunda bir tarama
        PollReactor(const PollReactor&); // Kopyalama constructor'ı, bu sınıfın kopyalanmasını engellemek için private ve tanımsız bırakılmıştır
        PollReactor& operator=(const PollReactor&); // Atama operatorü, bu sınıfın kopyalanmasını engellemek için private ve tanımsız bırakılmıştır

        struct ExternalFd
        {
            int fd;
            short events;
            FdCallback callback;
            void *ctx;
        
            ExternalFd() : fd(-1), events(0), callback(NULL), ctx(NULL) {}
        };

        
        static const int MAX_EXTERNAL_FDS = 1024;
        ExternalFd _externalFds[MAX_EXTERNAL_FDS];
};

#endif