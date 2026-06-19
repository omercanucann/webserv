#include "Pollreactor.hpp"

PollReactor::PollReactor(): _slot_count(0), _server_count(0), _on_data(NULL), _on_write(NULL), _on_close(NULL), _cb_ctx(NULL), _running(false), _sweep_counter(0)
{
    ft_memset(_pollfds, 0, sizeof(_pollfds)); // ne işe yarar? : _pollfds dizisini sıfırlar, bu dizide her slotun fd ve events bilgisi tutulur, başlangıçta tüm fd'ler -1 olarak işaretlenir ve events sıfırlanır
    for (int i = 0; i < MAX_SLOTS; ++i) // Tüm slotların fd'sini -1 yaparak başlangıçta boş olduklarını belirtiriz
        _pollfds[i].fd = -1;
 
    for (int i = 0; i < MAX_SERVERS; ++i) // Tüm sunucu slotlarının fd'sini -1 yaparak başlangıçta boş olduklarını belirtiriz
        _servers[i].fd = -1;
}

PollReactor::~PollReactor()
{
    for (int i = 0; i < MAX_SLOTS; ++i) // Tüm slotları kapatır, bu genellikle istemci bağlantıları ve sunucu soketleri için geçerlidir, kaynak sızıntılarını önlemek için önemlidir
    {
        if (_slots[i].fd > 0) // Eğer slotun fd'si geçerliyse (yani açık bir bağlantı veya sunucu soketi varsa)
            SocketBinder::close_fd(_slots[i].fd); // SocketBinder sınıfının close_fd fonksiyonunu kullanarak fd'yi kapatır ve fd'yi -1 yapar, bu sayede kaynak sızıntılarını önleriz
    }
}

bool PollReactor::add_server(const std::string &host, uint16_t port) // Belirtilen host ve port üzerinde bir dinleyici soketi açar ve bu soketin dosya tanıtıcısını döndürür. Eğer başarısız olursa, false döndürür.
{
    int listen_fd;
    int slot_index;

    if (_server_count >= MAX_SERVERS) // Maksimum sunucu sayısına ulaşıldıysa, yeni sunucu eklenemez, bu durumda false döndürülür ve ekleme işlemi başarısız olur. 
    // Bu sınırlama, sistem kaynaklarının aşırı kullanılmasını önlemek ve yönetilebilir bir sunucu sayısı sağlamak için konulmuştur.
    {
        std::cout << "[PollReactor] Maksimum sunucu sayısına ulaşıldı (" << MAX_SERVERS << ")" << std::endl;
        return (false);
    }
 
    listen_fd = SocketBinder::open_listener(host, port); // SocketBinder sınıfının open_listener fonksiyonunu kullanarak belirtilen host ve port üzerinde bir dinleyici soketi açar, 
    // bu fonksiyon başarılı olursa açılan soketin dosya tanıtıcısını döndürür, başarısız olursa -1 döndürür
    if (listen_fd < 0) // Dinleyici soketi açılamadıysa, bu genellikle belirtilen portun zaten kullanımda olması veya izin sorunları gibi durumlarda olur, bu durumda false döndürülür ve ekleme işlemi başarısız olur
        return (false);
 
    _servers[_server_count].host = host; // Yeni sunucu bilgilerini _servers dizisine kaydeder, bu dizi her sunucu için host, port ve fd bilgisi tutar, bu bilgiler daha sonra bağlantıları kabul etmek ve yönetmek için kullanılır
    _servers[_server_count].port = port; // Yeni sunucu bilgilerini _servers dizisine kaydeder, bu dizi her sunucu için host, port ve fd bilgisi tutar, bu bilgiler daha sonra bağlantıları kabul etmek ve yönetmek için kullanılır
    _servers[_server_count].fd   = listen_fd; // Yeni sunucu bilgilerini _servers dizisine kaydeder, bu dizi her sunucu için host, port ve fd bilgisi tutar, bu bilgiler daha sonra bağlantıları kabul etmek ve yönetmek için kullanılır
    _server_count++; // Sunucu sayısını artırır, bu sayede bir sonraki sunucu eklendiğinde doğru index'e kaydedilir
 
    slot_index = _find_empty_slot(); // Bağlantı havuzunda boş bir slot bulur, bu fonksiyon genellikle mevcut slot sayısına göre ilk boş slotu döndürür, eğer tüm slotlar doluysa -1 döndürür
    if (slot_index < 0) // Eğer boş slot bulunamazsa, bu genellikle bağlantı havuzunun tamamen dolu olduğu anlamına gelir, bu durumda açılan dinleyici soketi kapatılır ve false döndürülür, böylece ekleme işlemi başarısız olur
    {
        std::cout << "[PollReactor] Sunucu fd için slot bulunamadı" << std::endl;
        SocketBinder::close_fd(listen_fd); // Açılan dinleyici soketi kapatılır, bu kaynak sızıntılarını önlemek için önemlidir
        return (false);
    }
 
    _slots[slot_index].state       = ConnectState_READING; // Yeni sunucu slotu, bağlantıları kabul etmek için okuma durumunda başlatılır, bu sayede bu slotta gelen bağlantılar accept() ile yeni istemci slotlarına atanır
    _slots[slot_index].fd          = listen_fd; // Yeni sunucu slotunun fd'si olarak açılan dinleyici soketin dosya tanıtıcısı atanır, bu sayede bu slot üzerinden gelen bağlantılar yönetilir
    _slots[slot_index].is_server   = true; // Bu slotun bir sunucu soketi olduğunu belirtir, bu sayede bu slot üzerinden gelen bağlantılar accept() ile yeni istemci slotlarına atanır, 
    // sunucu soketleri doğrudan veri okumaz veya yazmaz, sadece bağlantıları kabul eder
    _slots[slot_index].last_active = time(NULL); // Sunucu slotunun son aktif zamanını şu anki zaman olarak ayarlar, bu bilgi genellikle bağlantı zaman aşımı yönetimi için kullanılır, 
    // ancak sunucu soketleri genellikle zaman aşımına uğramazlar
 
    _pollfds[slot_index].fd     = listen_fd; // Pollfd yapısında bu slotun fd'si olarak açılan dinleyici soketin dosya tanıtıcısı atanır, bu sayede poll() fonksiyonu bu fd üzerinden gelen olayları izler
    _pollfds[slot_index].events = POLLIN; // Pollfd yapısında bu slotun events alanı POLLIN olarak ayarlanır, bu sayede poll() fonksiyonu bu fd üzerinden gelen okuma olaylarını izler, 
    // sunucu soketleri için genellikle sadece okuma olayları izlenir, çünkü bu soketler sadece bağlantıları kabul eder    
 
    if (slot_index >= _slot_count) // Eğer bulunan slot index'i mevcut slot sayısından büyük veya eşitse, bu genellikle yeni bir slotun kullanılmaya başlandığı anlamına gelir, 
    // bu durumda _slot_count değeri güncellenir, böylece bir sonraki boş slot doğru index'e atanır
        _slot_count = slot_index + 1;
 
    std::cout << "[PollReactor] Sunucu slot[" << slot_index << "] fd=" << listen_fd << " " << host.c_str() << ":" << (unsigned)port << std::endl;
 
    return (true);
}

void PollReactor::set_callbacks(DataCallback on_data, DataCallback on_write, DataCallback on_close, void *context) // Yeni veri okunduğunda, slot yazılmaya hazır olduğunda ve bağlantı kapandığında çağrılacak 
//  callback fonksiyonlarını ve bu callback'lere geçirilecek context'i ayarlar, bu sayede üst katman uygulaması bu olaylara tepki verebilir 
{
    _on_data  = on_data;
    _on_write = on_write;
    _on_close = on_close;
    _cb_ctx   = context;
}

void PollReactor::run() // Poll döngüsünü başlatır, bu fonksiyon bloklayıcıdır, bu fonksiyon çağrıldığında olay döngüsü başlar ve bu döngü içinde poll() fonksiyonu kullanılarak tüm slotlar izlenir, 
//  gelen olaylara göre uygun işlemler yapılır, bu döngü request_shutdown() fonksiy
{
    struct pollfd pollList[MAX_SLOTS + MAX_EXTERNAL_FDS];
    int externalMap[MAX_SLOTS + MAX_EXTERNAL_FDS];
    int pollCount;
    int i;
    int processed;
    int n_ready;
    short rev;

    if (_server_count == 0) // Eğer hiç sunucu eklenmediyse, olay döngüsünü başlatmanın anlamı yoktur, bu durumda bir uyarı mesajı yazdırılır ve fonksiyon sonlandırılır, böylece uygulama gereksiz yere kaynak tüketmez
    {
        std::cout << "[PollReactor] Hiç sunucu eklenmedi, çıkılıyor" << std::endl;
        return;
    }
 
    _running = true; // Olay döngüsünü başlatır, bu bayrak true olduğu sürece döngü devam eder, request_shutdown() fonksiyonu çağrıldığında bu bayrak false yapılır ve döngü güvenli bir şekilde sonlanır
    std::cout << "[PollReactor] Olay döngüsü başlatıldı (" << _server_count << " sunucu)" << std::endl;
 
    while (_running) // Olay döngüsü, bu döngü içinde poll() fonksiyonu kullanılarak tüm slotlar izlenir, gelen olaylara göre uygun işlemler yapılır, bu döngü request_shutdown() fonksiyonu çağrıldığında sonlanır
    {
        pollCount = 0;

        i = 0;
        while (i < MAX_SLOTS + MAX_EXTERNAL_FDS)
        {
            externalMap[i] = -1;
            i++;
        }

        i = 0;
        while (i < _slot_count)
        {
            pollList[pollCount] = _pollfds[i];
            pollCount++;
            i++;
        }

        i = 0;
        while (i < MAX_EXTERNAL_FDS)
        {
            if (_externalFds[i].fd >= 0)
            {
                pollList[pollCount].fd = _externalFds[i].fd;
                pollList[pollCount].events = _externalFds[i].events;
                pollList[pollCount].revents = 0;
                externalMap[pollCount] = i;
                pollCount++;
            }
            i++;
        }

        n_ready = poll(pollList, (nfds_t)pollCount, POLL_TIMEOUT_MS);

        i = 0;
        while (i < _slot_count)
        {
            _pollfds[i].revents = pollList[i].revents;
            i++;
        }

        if (n_ready < 0) // Eğer poll() fonksiyonu hata döndürürse, bu genellikle bir sistem çağrısı hatasıdır, bu durumda hata mesajı yazdırılır ve döngü sonlandırılır, 
        // böylece uygulama hatalı durumlarda gereksiz yere kaynak tüketmez
        {
            if (errno == EINTR) // Eğer poll() fonksiyonu bir sinyal tarafından kesilirse, bu genellikle normal bir durumdur ve bu durumda döngü devam eder, böylece uygulama sinyal kesintilerinden etkilenmez 
            //  EINTR hatası, bir sistem çağrısının bir sinyal tarafından kesildiğini belirtir, bu durumda genellikle döngü devam eder ve poll() fonksiyonu tekrar çağrılır, böylece uygulama sinyal kesintilerinden etkilenmez
                continue;
            std::cerr << "[PollReactor] poll: " << strerror(errno) << std::endl;
            break;
        }
 
        if (n_ready == 0) // Eğer poll() fonksiyonu zaman aşımı nedeniyle 0 döndürürse, bu genellikle izlenen fd'ler üzerinde herhangi bir olay gerçekleşmediği anlamına gelir, 
        //  bu durumda zaman aşımı mesajı yazdırılır ve bağlantı zaman aşımı yönetimi için _sweep_timeouts() fonksiyonu çağrılır, böylece boşta kalan bağlantılar kapatılır ve kaynaklar serbest bırakılır, 
        //  bu sayede uygulama gereksiz yere kaynak tüketmez ve bağlantı yönetimi etkin bir şekilde yapılır     
        {
            _sweep_timeouts(); // Bağlantı zaman aşımı yönetimi için _sweep_timeouts() fonksiyonu çağrılır, bu fonksiyon tüm slotları tarar ve boşta kalma süresi 
            // CONNECT_IDLE_TIMEOUT'u aşan istemci bağlantılarını kapatır, bu sayede boşta kalan bağlantılar kapatılır ve kaynaklar serbest bırakılır, 
            // böylece uygulama gereksiz yere kaynak tüketmez ve bağlantı yönetimi etkin bir şekilde yapılır
            continue; // Zaman aşımı durumunda başka işlem yapmaya gerek yoktur, bu durumda döngü devam eder ve poll() fonksiyonu tekrar çağrılır, 
            //böylece uygulama zaman aşımı durumlarında gereksiz yere kaynak tüketmez ve bağlantı yönetimi etkin bir şekilde yapılır
        }
 
        processed = 0; // Gerçekleşen olayların sayısını takip etmek için kullanılır, bu değişken n_ready kadar olay işlendikten sonra döngüden çıkmak için kullanılır, 
        // böylece uygulama gereksiz yere kaynak tüketmez ve olay işleme etkin bir şekilde yapılır
        for (int i = 0; i < _slot_count && processed < n_ready; ++i) // Tüm slotları tarar, bu döngü içinde poll() fonksiyonu tarafından işaretlenen olaylar işlenir, 
        // böylece uygulama gelen olaylara tepki verebilir ve kaynakları verimli kullanır
        {
            rev = _pollfds[i].revents; // Bu slotta gerçekleşen olayları rev değişkenine atar, bu değişken POLLIN, POLLOUT, POLLHUP, POLLERR gibi olayları içerebilir, 
            // bu sayede uygulama gelen olaylara tepki verebilir ve kaynakları verimli kullanır
            if (rev == 0) // Eğer bu slotta herhangi bir olay gerçekleşmemişse, bu genellikle normal bir durumdur ve bu durumda döngü devam eder, böylece uygulama gereksiz yere kaynak tüketmez ve olay işleme etkin bir şekilde yapılır
                continue;
            processed++; // Gerçekleşen olayların sayısını artırır, bu sayede n_ready kadar olay işlendikten sonra döngüden çıkmak için kullanılır, 
            // böylece uygulama gereksiz yere kaynak tüketmez ve olay işleme etkin bir şekilde yapılır
 
            if (rev & (POLLHUP | POLLERR | POLLNVAL)) // Eğer bu slotta POLLHUP, POLLERR veya POLLNVAL gibi hatalı durumlar gerçekleşmişse, bu genellikle bağlantının kapandığı veya bir hata oluştuğu anlamına gelir,
             // bu durumda eğer bu slot bir sunucu soketi değilse, bu slot kapatılır, bu sayede kaynak sızıntıları önlenir ve uygulama hatalı durumlarda gereksiz yere kaynak tüketmez, 
             // sunucu soketleri genellikle hatalı durumlarda kapatılmaz, çünkü bu soketler sadece bağlantıları kabul eder ve doğrudan veri okumaz veya yazmazlar, 
             // bu nedenle bu soketlerde hatalı durumlar genellikle bağlantı sorunlarıyla ilgili değildir ve bu soketlerin kapatılması genellikle istenmez, bu nedenle bu kontrol sadece istemci bağlantıları için yapılır, 
             // böylece uygulama hatalı durumlarda gereksiz yere kaynak tüketmez ve bağlantı yönetimi etkin bir şekilde yapılır
             // POLLHUP: Bağlantı kapandı, POLLERR: Bir hata oluştu, POLLNVAL: Geçersiz fd, bu durumlar genellikle bağlantının kapandığı veya bir hata oluştuğu anlamına gelir, 
             // bu durumda slot kapatılır, bu sayede kaynak sızıntıları önlenir ve uygulama hatalı durumlarda gereksiz yere kaynak tüketmez, sunucu soketleri genellikle hatalı durumlarda kapatılmaz, 
             // çünkü bu soketler sadece bağlantıları kabul eder ve doğrudan veri okumaz veya yazmazlar, bu nedenle bu soketlerde hatalı durumlar genellikle bağlantı sorunlarıyla ilgili değildir ve 
             // bu soketlerin kapatılması genellikle istenmez, bu nedenle bu kontrol sadece istemci bağlantıları için yapılır, böylece uygulama hatalı durumlarda gereksiz yere kaynak tüketmez ve 
              //bağlantı yönetimi etkin bir şekilde yapılır 
            // POLLERR: Bir hata oluştu, bu durum genellikle bağlantı sorunlarıyla ilgili olabilir, bu durumda slot kapatılır, bu sayede kaynak sızıntıları önlenir ve uygulama hatalı durumlarda gereksiz yere kaynak tüketmez,
            // sunucu soketleri genellikle hatalı durumlarda kapatılmaz, çünkü bu soketler sadece bağlantıları kabul eder ve doğrudan veri okumaz veya yazmazlar, 
            //bu nedenle bu soketlerde hatalı durumlar genellikle bağlantı sorunlarıyla ilgili değildir ve bu soketlerin kapatılması genellikle istenmez, bu nedenle bu kontrol sadece istemci bağlantıları için yapılır, 
            // böylece uygulama hatalı durumlarda gereksiz yere kaynak tüketmez ve bağlantı yönetimi etkin bir şekilde yapılır
            // POLLNVAL: Geçersiz fd, bu durum genellikle bir programlama hatası veya beklenmeyen bir durum nedeniyle oluşabilir, bu durumda slot kapatılır, bu sayede kaynak sızıntıları önlenir ve uygulama hatalı durumlarda gereksiz yere kaynak tüketmez,
            // sunucu soketleri genellikle hatalı durumlarda kapatılmaz, çünkü bu soketler sadece bağlantıları kabul eder ve doğrudan veri okumaz veya yazmazlar, 
            //bu nedenle bu soketlerde hatalı durumlar genellikle bağlantı sorunlarıyla ilgili değildir ve bu soketlerin kapatılması genellikle istenmez, bu nedenle bu kontrol sadece istemci bağlantıları için yapılır, 
            // böylece uygulama hatalı durumlarda gereksiz yere kaynak tüketmez ve bağlantı yönetimi etkin bir şekilde yapılır
            {
                if (!_slots[i].is_server) // Eğer bu slot bir sunucu soketi değilse, bu slot kapatılır, bu sayede kaynak sızıntıları önlenir ve uygulama hatalı durumlarda gereksiz yere kaynak tüketmez, 
                // sunucu soketleri genellikle hatalı durumlarda kapatılmaz, çünkü bu soketler sadece bağlantıları kabul eder ve doğrudan veri okumaz veya yazmazlar 
                {
                    std::cout << "[PollReactor] Slot[" << i << "] hata/kapat (rev=0x" << std::hex << (unsigned)rev << std::dec << ")" << std::endl;
                    _close_slot(i); // Bu slot kapatılır, bu genellikle istemci bağlantısı için geçerlidir, sunucu soketleri için kullanılmaz, çünkü bu soketler sadece bağlantıları kabul eder ve doğrudan veri okumaz veya yazmazlar
                }
                continue;
            }
 
            if (rev & POLLIN) // Eğer bu slotta POLLIN olayı gerçekleşmişse, bu genellikle bu slot üzerinden veri okunabileceği anlamına gelir, bu durumda eğer bu slot bir sunucu soketi ise, 
            // bu slot üzerinden yeni bağlantılar kabul edilir,
             // bu genellikle accept() çağrısı yaparak yeni bir istemci slotu oluşturur, eğer bu slot bir sunucu soketi değilse, bu slot üzerinden veri okunmaya çalışılır, 
             // bu genellikle recv() çağrısı yaparak okunan verileri slotun readbuffer'ına ekler ve üst katmana iletmek için on_data callback'ini çağırır, 
             // böylece uygulama gelen verilere tepki verebilir ve kaynakları verimli kullanır
             // POLLIN: Veri okunabilir, bu durum genellikle bu slot üzerinden veri okunabileceği anlamına gelir, bu durumda eğer bu slot bir sunucu soketi ise, bu slot üzerinden yeni bağlantılar kabul edilir,
            {
                if (_slots[i].is_server) // Eğer bu slot bir sunucu soketi ise, bu slot üzerinden yeni bağlantılar kabul edilir, bu genellikle accept() çağrısı yaparak yeni bir istemci slotu oluşturur,
                // bu sayede uygulama gelen bağlantılara tepki verebilir ve kaynakları verimli kullanır, sunucu soketleri doğrudan veri okumaz veya yazmaz, sadece bağlantıları kabul eder, 
                // bu nedenle bu kontrol yapılır, böylece uygulama gelen bağlantılara tepki verebilir
                    _accept_client(i);   // Yeni bağlantı kabul edilir, bu genellikle accept() çağrısı yaparak yeni bir istemci slotu oluşturur, bu sayede uygulama gelen bağlantılara tepki verebilir ve kaynakları verimli kullanır
                else
                    _read_slot(i);       // Mevcut bağlantıdan veri okunmaya çalışılır, bu genellikle recv() çağrısı yaparak okunan verileri slotun readbuffer'ına ekler ve üst katmana iletmek için on_data callback'ini çağırır, 
                    // böylece uygulama gelen verilere tepki verebilir ve kaynakları verimli kullanır   
            }

            if (rev & POLLOUT) // Eğer bu slotta POLLOUT olayı gerçekleşmişse, bu genellikle bu slot üzerinden veri yazılabileceği anlamına gelir, bu durumda eğer bu slot bir sunucu soketi değilse, 
            //bu slot üzerinden veri yazılmaya çalışılır,
             // bu genellikle send() çağrısı yaparak writebuffer'daki verileri gönderir, gönderilen byte kadar write_position'ı artırır, eğer tüm veri gönderildiyse on_write callback'ini çağırır,
             // böylece uygulama yazma işlemi tamamlandığında tepki verebilir ve kaynakları verimli kullanır, sunucu soketleri doğrudan veri okumaz veya yazmaz, sadece bağlantıları kabul eder,
             // bu nedenle bu kontrol yapılır, böylece uygulama yazma işlemi tamamlandığında tepki verebilir
             // POLLOUT: Veri yazılabilir, bu durum genellikle bu slot üzerinden veri yazılabileceği anlamına gelir, bu durumda eğer bu slot bir sunucu soketi değilse, bu slot üzerinden veri yazılmaya çalışılır, bu genellikle send() çağrısı yaparak writebuffer'daki verileri gönderir, gönderilen byte kadar write_position'ı artırır, eğer tüm veri gönderildiyse on_write callback'ini çağırır
            {
                if (!_slots[i].is_server) // Eğer bu slot bir sunucu soketi değilse, bu slot üzerinden veri yazılmaya çalışılır, bu genellikle send() çağrısı yaparak writebuffer'daki verileri gönderir, 
                // gönderilen byte kadar write_position'ı artırır, eğer tüm veri gönderildiyse on_write callback'ini çağırır, böylece uygulama yazma işlemi tamamlandığında tepki verebilir 
                // ve kaynakları verimli kullanır, sunucu soketleri doğrudan veri okumaz veya yazmaz, sadece bağlantıları kabul eder, bu nedenle bu kontrol yapılır, böylece uygulama yazma işlemi tamamlandığında tepki verebilir
                    _write_slot(i); // Bu slot üzerinden veri yazılmaya çalışılır, bu genellikle send() çağrısı yaparak writebuffer'daki verileri gönderir, 
                    //gönderilen byte kadar write_position'ı artırır, eğer tüm veri gönderildiyse on_write callback'ini çağırır, böylece uygulama yazma işlemi tamamlandığında tepki verebilir ve kaynakları verimli kullanır
            }
        }

        i = _slot_count;
        while (i < pollCount)
        {
            int extIndex;
            short extRev;
        
            extIndex = externalMap[i];
            extRev = pollList[i].revents;
        
            if (extIndex >= 0 && extRev != 0 && _externalFds[extIndex].callback != NULL)
            {
                _externalFds[extIndex].callback(
                    _externalFds[extIndex].ctx,
                    _externalFds[extIndex].fd,
                    extRev
                );
            }
            i++;
        }
        
        _sweep_counter++; // Bağlantı zaman aşımı yönetimi için sayaç artırılır, bu sayaç belirli bir değere ulaştığında _sweep_timeouts() fonksiyonu çağrılır, böylece boşta kalan bağlantılar kapatılır ve kaynaklar serbest bırakılır,
        if (_sweep_counter >= SWEEP_INTERVAL) // Eğer sayaç belirli bir değere ulaşmışsa, bu genellikle belirli bir süre geçtiği anlamına gelir, bu durumda bağlantı zaman aşımı yönetimi için 
        // _sweep_timeouts() fonksiyonu çağrılır, böylece boşta kalan bağlantılar kapatılır ve kaynaklar serbest bırakılır, böylece uygulama gereksiz yere kaynak tüketmez ve bağlantı yönetimi etkin bir şekilde yapılır
        {
            _sweep_timeouts(); // Bağlantı zaman aşımı yönetimi için _sweep_timeouts() fonksiyonu çağrılır, bu fonksiyon tüm slotları tarar ve boşta kalma süresi CONNECT_IDLE_TIMEOUT'u aşan istemci bağlantılarını kapatır, 
            // bu sayede boşta kalan bağlantılar kapatılır ve kaynaklar serbest bırakılır, böylece uygulama gereksiz yere kaynak tüketmez ve bağlantı yönetimi etkin bir şekilde yapılır
            _sweep_counter = 0; // Sayaç sıfırlanır, böylece bir sonraki zaman aşımı kontrolü için sayaç tekrar artırılmaya başlanır, bu sayede uygulama gereksiz yere kaynak tüketmez ve bağlantı yönetimi etkin bir şekilde yapılır
        }
    }
    std::cout << "[PollReactor] Döngü sonlandı" << std::endl;
}
 
void PollReactor::request_shutdown() // Poll döngüsünü güvenli bir şekilde sonlandırmak için çağrılır, bu fonksiyon çağrıldığında _running bayrağı false yapılır ve böylece olay döngüsü güvenli bir şekilde sonlanır,
{
    _running = false; // Olay döngüsünü sonlandırmak için _running bayrağı false yapılır, bu sayede run() fonksiyonundaki döngü güvenli bir şekilde sonlanır, bu fonksiyon genellikle uygulama kapanırken veya yeniden başlatılırken çağrılır, böylece kaynak sızıntıları önlenir ve uygulama düzgün bir şekilde kapanır
}
 
void PollReactor::_accept_client(int server_slot_index) // Belirli bir sunucu slotuna gelen yeni bağlantıyı kabul eder, bu genellikle accept() çağrısı yapar ve yeni bir istemci slotu oluşturur, 
// bu sayede uygulama gelen bağlantılara tepki verebilir ve kaynakları verimli kullanır
{
    struct sockaddr_in client_addr;
    socklen_t          addr_len;
    int server_fd;
    int slot_index;
    char ip_buf[INET_ADDRSTRLEN]; // IP adresini string olarak tutmak için yeterli boyutta bir buffer, INET_ADDRSTRLEN genellikle 16 bayt olarak tanımlanır, bu sayede IPv4 adreslerini güvenli bir şekilde tutabiliriz
 
    addr_len = sizeof(client_addr); // client_addr yapısının boyutunu belirtir, bu bilgi accept() fonksiyonuna geçilir, böylece accept() fonksiyonu client_addr yapısına doğru şekilde doldurabilir,
    // bu sayede uygulama gelen bağlantıların IP adresi ve port bilgilerini alabilir
    server_fd = _slots[server_slot_index].fd; // Bu sunucu slotunun fd'sini alır, bu fd üzerinden accept() çağrısı yaparak yeni bağlantıları kabul eder, bu sayede uygulama gelen bağlantılara tepki verebilir ve 
    // kaynakları verimli kullanır
 
    while (true)
    {
        int client_fd;
 
        client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &addr_len); // accept() fonksiyonu, server_fd üzerinden gelen yeni bağlantıyı kabul eder, bu fonksiyon başarılı olursa yeni bağlantının 
        // dosya tanıtıcısını döndürür, başarısız olursa -1 döndürür,
        if (client_fd < 0) // Eğer accept() fonksiyonu hata döndürürse, bu genellikle bir sistem çağrısı hatasıdır, bu durumda hata mesajı yazdırılır ve döngü sonlandırılır, 
        // böylece uygulama hatalı durumlarda gereksiz yere kaynak tüketmez
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK) // Eğer accept() fonksiyonu EAGAIN veya EWOULDBLOCK hatası döndürürse, bu genellikle bu sunucu soketinde şu anda kabul edilecek yeni bir bağlantı olmadığı anlamına gelir,
            // EAGAIN: Kaynak geçici olarak kullanılamıyor, EWOULDBLOCK: İşlem bloklanırdı, bu durumlar genellikle normaldir ve bu durumda döngü sonlandırılır, böylece uygulama gereksiz yere kaynak tüketmez ve 
            // olay işleme etkin bir şekilde yapılır
                break;
            std::cerr << "[PollReactor] accept: " << strerror(errno) << std::endl;
            break;
        }
 
        if (!SocketBinder::set_nonblocking(client_fd)) // Yeni kabul edilen bağlantının dosya tanıtıcısını non-blocking moda alır, bu genellikle SocketBinder sınıfının set_nonblocking fonksiyonu kullanılarak yapılır, bu fonksiyon başarılı olursa true döndürür, başarısız olursa false döndürür,
         // eğer bu işlem başarısız olursa, açılan client_fd kapatılır ve döngü devam eder, böylece uygulama hatalı durumlarda gereksiz yere kaynak tüketmez ve bağlantı yönetimi etkin bir şekilde yapılır
        {
            SocketBinder::close_fd(client_fd); // Eğer set_nonblocking işlemi başarısız olursa, açılan client_fd kapatılır, bu kaynak sızıntılarını önlemek için önemlidir, 
            // böylece uygulama hatalı durumlarda gereksiz yere kaynak tüketmez ve bağlantı yönetimi etkin bir şekilde yapılır
            continue;
        }

        slot_index = _find_empty_slot(); // Bağlantı havuzunda boş bir slot bulur, bu fonksiyon genellikle mevcut slot sayısına göre ilk boş slotu döndürür, eğer tüm slotlar doluysa -1 döndürür, 
         // eğer boş slot bulunamazsa, bu genellikle bağlantı havuzunun tamamen dolu olduğu anlamına gelir, bu durumda açılan client_fd kapatılır ve döngü devam eder, böylece uygulama hatalı durumlarda gereksiz yere kaynak tüketmez 
         // ve bağlantı yönetimi etkin bir şekilde yapılır
        if (slot_index < 0) // Eğer boş slot bulunamazsa, bu genellikle bağlantı havuzunun tamamen dolu olduğu anlamına gelir, bu durumda açılan client_fd kapatılır ve döngü devam eder, 
        // böylece uygulama hatalı durumlarda gereksiz yere kaynak tüketmez ve bağlantı yönetimi etkin bir şekilde yapılır
        {
            std::cout << "[PollReactor] Slot doldu, bağlantı reddediliyor" << std::endl;
            close(client_fd); 
            continue;
        }

        inet_ntop(AF_INET, &client_addr.sin_addr, ip_buf, sizeof(ip_buf)); // Gelen bağlantının IP adresini string formatına çevirir, bu genellikle inet_ntop fonksiyonu kullanılarak yapılır, bu fonksiyon başarılı olursa 
        // IP adresini ip_buf'a yazar ve ip_buf'un adresini döndürür, başarısız olursa NULL döndürür,
         // bu sayede uygulama gelen bağlantının IP adresini alabilir ve yönetebilir
         // İNET_NTOP: Ağ adresini sunucu adresine dönüştürür, AF_INET: IPv4 adresi, &client_addr.sin_addr: Gelen bağlantının IP adresi, ip_buf: Dönüştürülen IP adresinin yazılacağı buffer, sizeof(ip_buf): Buffer'ın boyutu, 
         // bu sayede uygulama gelen bağlantının IP adresini alabilir ve yönetebilir

        ConnectionSlot &slot     = _slots[slot_index]; // Yeni istemci bağlantısı için slotu doldurur, bu slot genellikle bağlantının durumunu, dosya tanıtıcısını, uzak IP adresini ve portunu, yazma ve okuma buffer'larını tutar,
        slot.state               = ConnectState_READING; // Yeni istemci bağlantısı için slotun durumunu okuma olarak ayarlar, bu sayede bu slot üzerinden gelen veriler okunmaya çalışılır, bu genellikle recv() çağrısı yaparak 
        // okunan verileri slotun readbuffer'ına ekler ve üst katmana iletmek için on_data callback'ini çağırır, böylece uygulama gelen verilere tepki verebilir ve kaynakları verimli kullanır
        slot.fd                  = client_fd; // Yeni istemci bağlantısı için slotun dosya tanıtıcısını client_fd olarak ayarlar, bu sayede bu slot üzerinden gelen veriler okunmaya çalışılır ve yazılmaya çalışılır, 
        // bu genellikle recv() ve send() çağrıları yaparak okunan verileri slotun readbuffer'ına ekler ve slotun writebuffer'ındaki verileri gönderir, böylece uygulama gelen verilere tepki verebilir ve kaynakları verimli kullanır
        slot.is_server           = false; // Bu slotun bir sunucu soketi olmadığını belirtir, bu sayede bu slot üzerinden gelen veriler okunmaya çalışılır ve yazılmaya çalışılır, bu genellikle recv() ve send() 
        // çağrıları yaparak okunan verileri slotun readbuffer'ına ekler ve slotun writebuffer'ındaki verileri gönderir, böylece uygulama gelen verilere tepki verebilir ve kaynakları verimli kullanır, 
        // sunucu soketleri doğrudan veri okumaz veya yazmaz, sadece bağlantıları kabul eder, bu nedenle bu kontrol yapılır, böylece uygulama gelen verilere tepki verebilir
        slot.remote_ip           = ip_buf; // Yeni istemci bağlantısının uzak IP adresini ip_buf'dan alır ve slotun remote_ip alanına atar, bu sayede uygulama gelen bağlantının IP adresini alabilir ve yönetebilir
        slot.remote_port         = ntohs(client_addr.sin_port); // Yeni istemci bağlantısının uzak portunu client_addr.sin_port'dan alır, bu değer ağ byte sırasındadır, bu nedenle ntohs() fonksiyonu kullanılarak 
        // host byte sırasına çevrilir ve slotun remote_port alanına atanır, bu sayede uygulama gelen bağlantının port bilgisine sahip olabilir ve yönetebilir
        slot.self_index            = slot_index;   // Bridge ve üst katman için
        slot.origin_server_fd    = server_fd;  // Bridge ve üst katman için
        slot.request_complete    = false; // Yeni bağlantı için isteğin tamamlanmadığını belirtir, bu genellikle HTTP gibi protokollerde bir isteğin tam olarak alınıp işlenmesi gerektiği durumlarda kullanılır, 
        // bu sayede uygulama gelen verilerin tam olarak alınıp işlenmesini yönetebilir
        slot.readbuffer.clear(); // Okuma buffer'ını temizler, bu genellikle yeni bir bağlantı için eski verilerin kalmaması için yapılır, bu sayede uygulama gelen verilerin tam olarak alınıp işlenmesini yönetebilir
        slot.writebuffer.clear(); // Yazma buffer'ını temizler, bu genellikle yeni bir bağlantı için eski verilerin kalmaması için yapılır, bu sayede uygulama gelen verilerin tam olarak alınıp işlenmesini yönetebilir
        slot.write_position      = 0; // Yazma pozisyonunu sıfırlar, bu genellikle yeni bir bağlantı için yazma işleminin başından başlanması gerektiği durumlarda kullanılır, bu sayede uygulama gelen verilerin tam olarak 
        // alınıp işlenmesini yönetebilir
        slot.touch(); // Slotun son aktif zamanını günceller, bu genellikle bağlantı zaman aşımı yönetimi için kullanılır, bu sayede uygulama gelen verilerin tam olarak alınıp işlenmesini yönetebilir
 
        _pollfds[slot_index].fd      = client_fd; // Pollfd yapısında bu slotun fd'si olarak yeni kabul edilen istemci bağlantısının dosya tanıtıcısı atanır, bu sayede poll() fonksiyonu bu fd üzerinden gelen olayları izler, 
        // bu sayede uygulama gelen verilere tepki verebilir ve kaynakları verimli kullanır
        _pollfds[slot_index].events  = POLLIN; // Pollfd yapısında bu slotun events alanı POLLIN olarak ayarlanır, bu sayede poll() fonksiyonu bu fd üzerinden gelen okuma olaylarını izler, bu genellikle recv() çağrısı yaparak okunan verileri slotun readbuffer'ına ekler ve üst katmana iletmek için on_data callback'ini çağırır,
        // böylece uygulama gelen verilere tepki verebilir ve kaynakları verimli kullanır POLLIN: Veri okunabilir, bu durum genellikle bu slot üzerinden veri okunabileceği anlamına gelir, bu durumda recv() 
        // çağrısı yaparak okunan verileri slotun readbuffer'ına ekler ve üst katmana iletmek için on_data callback'ini çağırır, böylece uygulama gelen verilere tepki verebilir ve kaynakları verimli kullanır
        _pollfds[slot_index].revents = 0; // Pollfd yapısında bu slotun revents alanı sıfırlanır, bu genellikle yeni bir bağlantı için önceki olayların temizlenmesi için yapılır, bu sayede uygulama gelen verilere 
        // tepki verebilir ve kaynakları verimli kullanır
 
        if (slot_index >= _slot_count) // Eğer yeni kabul edilen bağlantı için bulunan slot index'i mevcut slot sayısından büyük veya eşitse, bu genellikle yeni bir slotun kullanılmaya başlandığı anlamına gelir, 
        // bu durumda _slot_count değeri güncellenir,
            _slot_count = slot_index + 1;
 
        std::cout << "[PollReactor] Yeni bağlantı: slot[" << slot_index << "] fd=" << client_fd << " " << ip_buf << ":" << (unsigned)ntohs(client_addr.sin_port) << std::endl;
    }
}
 
void PollReactor::_read_slot(int slot_index) // Belirli bir slot üzerinden veri okumaya çalışır, bu genellikle recv() çağrısı yaparak okunan verileri slotun readbuffer'ına ekler ve üst katmana iletmek için 
// on_data callback'ini çağırır,
{
    char  buf[READ_CHUNK]; // Okuma işlemi için geçici bir buffer, bu buffer genellikle belirli bir boyutta tanımlanır (örneğin 4096 bayt), bu sayede okunan veriler bu buffer'a alınır ve daha sonra slotun readbuffer'ına eklenir,
    bool  got_data;  // Veri alınıp alınmadığını takip etmek için kullanılan bir bayrak, bu bayrak okuma işlemi sırasında veri alındığında true olarak ayarlanır, bu sayede on_data callback'inin doğru şekilde çağrılması sağlanır
    bool  connect_closed; // Bağlantının kapatılıp kapatılmadığını takip etmek için kullanılan bir bayrak, bu bayrak okuma işlemi sırasında bağlantının kapandığı tespit edildiğinde true olarak ayarlanır, 
    // bu sayede bağlantı kapatıldığında slotun doğru şekilde kapatılması sağlanır
 
    ConnectionSlot &slot = _slots[slot_index]; // Okuma işlemi yapılacak slotu alır, bu slot genellikle bağlantının durumunu, dosya tanıtıcısını, uzak IP adresini ve portunu, yazma ve okuma buffer'larını tutar, 
    // bu sayede uygulama gelen verilerin tam olarak alınıp işlenmesini yönetebilir
    got_data   = false;  // Veri alınıp alınmadığını takip etmek için kullanılan bayrak başlangıçta false olarak ayarlanır, bu sayede on_data callback'inin doğru şekilde çağrılması sağlanır
    connect_closed = false; // Bağlantının kapatılıp kapatılmadığını takip etmek için kullanılan bayrak başlangıçta false olarak ayarlanır, bu sayede bağlantı kapatıldığında slotun doğru şekilde kapatılması sağlanır

    while (true) // Okuma işlemi için sonsuz bir döngü, bu döngü içinde recv() çağrısı yaparak okunan veriler slotun readbuffer'ına eklenir, eğer recv() çağrısı EAGAIN veya EWOULDBLOCK hatası döndürürse, 
    // bu genellikle okunacak daha fazla veri olmadığı anlamına gelir ve bu durumda döngü sonlandırılır, böylece uygulama gereksiz yere kaynak tüketmez ve olay işleme etkin bir şekilde yapılır
    {
        ssize_t n = read(slot.fd, buf, sizeof(buf)); // read() fonksiyonu, slotun dosya tanıtıcısından buf'a sizeof(buf) kadar veri okumaya çalışır, bu fonksiyon başarılı olursa okunan byte sayısını döndürür
 
        if (n > 0) // Eğer read() fonksiyonu pozitif bir değer döndürürse, bu genellikle okunan byte sayısını belirtir, bu durumda okunan veriler slotun readbuffer'ına eklenir, got_data bayrağı true olarak ayarlanır ve 
        // slotun son aktif zamanı güncellenir, 
         // bu sayede on_data callback'inin doğru şekilde çağrılması sağlanır ve uygulama gelen verilerin tam olarak alınıp işlenmesini yönetebilir
         // n > 0: Okunan byte sayısı, bu durum genellikle okunan byte sayısını belirtir, bu durumda okunan veriler slotun readbuffer'ına eklenir, got_data bayrağı true olarak ayarlanır ve slotun son aktif zamanı güncellenir, 
         // böylece on_data callback'inin doğru şekilde çağrılması sağlanır ve uygulama gelen verilerin tam olarak alınıp işlenmesini yönetebilir
        {
            slot.readbuffer.insert(slot.readbuffer.end(), buf, buf + n); // Okunan verileri slotun readbuffer'ına ekler, bu genellikle std::vector<char> gibi bir yapıya veri eklemek için insert() fonksiyonu kullanılarak yapılır, bu sayede uygulama gelen verilerin tam olarak alınıp işlenmesini yönetebilir
             // buf: Okunan verilerin tutulduğu geçici buffer, n: Okunan byte sayısı, bu sayede uygulama gelen verilerin tam olarak alınıp işlenmesini yönetebilir
             // slot.readbuffer.insert(slot.readbuffer.end(), buf, buf + n): Okunan verileri slotun readbuffer'ına ekler, bu genellikle std::vector<char> gibi bir yapıya veri eklemek için insert() fonksiyonu kullanılarak yapılır, 
             // bu sayede uygulama gelen verilerin tam olarak alınıp işlenmesini yönetebilir
             // slot.readbuffer.end(): readbuffer'ın sonunu belirtir, buf: Okunan verilerin tutulduğu geçici buffer, n: Okunan byte sayısı, bu sayede uygulama gelen verilerin tam olarak alınıp işlenmesini yönetebilir
             // slot.readbuffer.insert(slot.readbuffer.end(), buf, buf + n): Okunan verileri slotun readbuffer'ına ekler, bu genellikle std::vector<char> gibi bir yapıya veri eklemek için insert() fonksiyonu kullanılarak yapılır, 
             // bu sayede uygulama gelen verilerin tam olarak alınıp işlenmesini yönetebilir
            got_data = true; // Veri alındığını belirtir, bu sayede on_data callback'inin doğru şekilde çağrılması sağlanır ve uygulama gelen verilerin tam olarak alınıp işlenmesini yönetebilir
            slot.touch(); // Slotun son aktif zamanını günceller, bu genellikle bağlantı zaman aşımı yönetimi için kullanılır, bu sayede uygulama gelen verilerin tam olarak alınıp işlenmesini yönetebilir
             // slot.touch(): Slotun son aktif zamanını günceller, bu genellikle bağlantı zaman aşımı yönetimi için kullanılır, böylece uygulama gelen verilerin tam olarak alınıp işlenmesini yönetebilir
             // slot.touch() fonksiyonu genellikle slotun içinde bir last_active_time gibi bir değişkeni güncelleyerek çalışır, bu sayede bağlantının ne kadar süredir aktif olduğunu takip edebiliriz ve 
             // belirli bir süre boyunca aktif olmayan bağlantıları kapatabiliriz, böylece uygulama gereksiz yere kaynak tüketmez ve bağlantı yönetimi etkin bir şekilde yapılır
             // slot.touch() fonksiyonu genellikle slotun içinde bir last_active_time gibi bir değişkeni güncelleyerek çalışır, bu sayede bağlantının ne kadar süredir aktif olduğunu takip edebiliriz ve belirli 
             // bir süre boyunca aktif olmayan bağlantıları kapatabiliriz
             // böylece uygulama gereksiz yere kaynak tüketmez ve bağlantı yönetimi etkin bir şekilde yapılır
             // slot.touch() fonksiyonu genellikle slotun içinde bir last_active_time gibi bir değişkeni güncelleyerek çalışır, bu sayede bağlantının ne kadar süredir aktif olduğunu takip edebiliriz ve 
             // belirli bir süre boyunca aktif olmayan bağlantıları
            continue;
        }
 
        if (n == 0)
        {
            connect_closed = true; // Karşı taraf bağlantıyı kapatmış, bu durumda bağlantının kapatıldığını belirtir, bu sayede slotun doğru şekilde kapatılması sağlanır ve uygulama gereksiz yere kaynak tüketmez
            break;
        }

        if (errno == EAGAIN || errno == EWOULDBLOCK) // Eğer read() fonksiyonu EAGAIN veya EWOULDBLOCK hatası döndürürse, bu genellikle okunacak daha fazla veri olmadığı anlamına gelir, bu durumda döngü sonlandırılır, 
        // böylece uygulama gereksiz yere kaynak tüketmez ve olay işleme etkin bir şekilde yapılır
            break; // Şimdilik okunacak başka veri yok, normal durum
 
        std::cout << "[PollReactor] read() slot[" << slot_index << "]: " << strerror(errno) << std::endl;
        connect_closed = true;
        break;
    }
 
    if (connect_closed)
    {
        _close_slot(slot_index);
        return;
    }
 
    if (got_data && _on_data) // Eğer veri alındıysa ve on_data callback'i tanımlıysa, bu callback çağrılır, bu sayede uygulama gelen verilere tepki verebilir ve kaynakları verimli kullanır
        _on_data(_cb_ctx, slot); // on_data callback'i çağrılır, bu callback genellikle uygulamanın gelen verileri işlemesi için kullanılır, bu sayede uygulama gelen verilere tepki verebilir ve kaynakları verimli kullanır
}
 
void PollReactor::_write_slot(int slot_index) // Belirli bir slot üzerinden veri yazmaya çalışır, bu genellikle send() çağrısı yaparak writebuffer'daki verileri gönderir, gönderilen byte kadar write_position'ı artırır, 
// eğer tüm veri gönderildiyse on_write callback'ini çağırır,
{
    ConnectionSlot &slot = _slots[slot_index]; // Yazma işlemi yapılacak slotu alır, bu slot genellikle bağlantının durumunu, dosya tanıtıcısını, uzak IP adresini ve portunu, yazma ve okuma buffer'larını tutar
 
    if (slot.writebuffer.empty() || slot.write_done()) // Eğer yazma buffer'ı boşsa veya yazma işlemi tamamlanmışsa, bu durumda yazma işlemi yapılmaz, bunun yerine poll olayları güncellenir, 
    // böylece uygulama gereksiz yere kaynak tüketmez ve olay işleme etkin bir şekilde yapılır
    {
        _sync_poll_events(slot_index); // Poll olaylarını günceller, bu genellikle POLLOUT olayını kaldırarak yapılır, böylece uygulama gereksiz yere kaynak tüketmez ve olay işleme etkin bir şekilde yapılır
         // _sync_poll_events(slot_index): Poll olaylarını günceller, bu genellikle POLLOUT olayını kaldırarak yapılır, böylece uygulama gereksiz yere kaynak tüketmez ve olay işleme etkin bir şekilde yapılır
         // _sync_poll_events() fonksiyonu genellikle slotun durumuna göre pollfd yapısındaki events alanını günceller, eğer slot yazma işlemi yapıyorsa ve yazma işlemi tamamlanmamışsa POLLOUT ekler, 
         // aksi halde POLLOUT'u kaldırır, böylece uygulama gereksiz yere kaynak tüketmez ve olay işleme etkin bir şekilde yapılır
         // _sync_poll_events() fonksiyonu genellikle slotun durumuna göre pollfd yapısındaki events alanını günceller, eğer slot yazma işlemi yapıyorsa ve yazma işlemi tamamlanmamışsa POLLOUT ekler, aksi halde POLLOUT'u kaldırır
         // böylece uygulama gereksiz yere kaynak tüketmez ve olay işleme etkin bir şekilde yapılır
        return;
    }

    while (!slot.write_done()) // Yazma işlemi tamamlanana kadar döngü, bu döngü içinde send() çağrısı yaparak writebuffer'daki veriler gönderilir, gönderilen byte kadar write_position'ı artırılır,
    // eğer send() çağrısı EAGAIN veya EWOULDBLOCK hatası döndürürse, bu genellikle gönderilecek daha fazla veri olmadığı anlamına gelir ve bu durumda döngü sonlandırılır, böylece uygulama gereksiz yere kaynak tüketmez 
    // ve olay işleme etkin bir şekilde yapılır
    {
        const char *data;
        size_t      remain;
        ssize_t     n;
 
        data   = slot.writebuffer.data() + slot.write_position; // Gönderilecek verinin başlangıç adresini alır, bu genellikle writebuffer'ın data() fonksiyonu kullanılarak yapılır, bu sayede uygulama yazma işlemi sırasında
        // hangi verilerin gönderileceğini yönetebilir
         // slot.writebuffer.data(): Yazma buffer'ının başlangıç adresi, slot.write_position: Gönderilen byte sayısı, bu sayede uygulama yazma işlemi sırasında hangi verilerin gönderileceğini yönetebilir
         // data: Gönderilecek verinin başlangıç adresi, bu sayede uygulama yazma işlemi sırasında hangi verilerin gönderileceğini yönetebilir
        remain = slot.pending_write(); // Gönderilmesi gereken toplam byte sayısını alır, bu genellikle writebuffer'daki toplam veri boyundan write_position'ı çıkararak hesaplanır, bu sayede uygulama yazma işlemi sırasında 
        // hangi verilerin gönderileceğini yönetebilir
         // slot.writebuffer.size(): Yazma buffer'ındaki toplam veri boyu, slot.write_position: Gönderilen byte sayısı, bu sayede uygulama yazma işlemi sırasında hangi verilerin gönderileceğini yönetebilir
        n = write(slot.fd, data, remain); // write() fonksiyonu, slotun dosya tanıtıcısından data adresindeki veriyi remain byte kadar göndermeye çalışır, bu fonksiyon başarılı olursa gönderilen byte sayısını döndürür, 
        //başarısız olursa -1 döndürür
         // write() fonksiyonu, slotun dosya tanıtıcısından data adresindeki veriyi remain byte kadar göndermeye çalışır, bu fonksiyon başarılı olursa gönderilen byte sayısını döndürür, başarısız olursa -1 döndürür
         // n: Gönderilen byte sayısı, bu durum genellikle gönderilen byte sayısını belirtir, bu sayede uygulama yazma işlemi sırasında hangi verilerin gönderildiğini yönetebilir
        if (n > 0) // Eğer write() fonksiyonu pozitif bir değer döndürürse, bu genellikle gönderilen byte sayısını belirtir, bu durumda write_position gönderilen byte kadar artırılır, slotun son aktif zamanı güncellenir 
        // ve döngü devam eder,
         // böylece uygulama yazma işlemi sırasında hangi verilerin gönderildiğini yönetebilir
         // n > 0: Gönderilen byte sayısı, bu durum genellikle gönderilen byte sayısını belirtir, bu durumda write_position gönderilen byte kadar artırılır, slotun son aktif zamanı güncellenir ve döngü devam eder, 
         //böylece uygulama yazma işlemi sırasında hangi verilerin gönderildiğini yönetebilir
        {
            slot.write_position += (size_t)n; // write_position gönderilen byte kadar artırılır, bu sayede uygulama yazma işlemi sırasında hangi verilerin gönderildiğini yönetebilir
             // slot.write_position: Gönderilen byte sayısı, n: Gönderilen byte sayısı, bu sayede uygulama yazma işlemi sırasında hangi verilerin gönderildiğini yönetebilir
            slot.touch(); // Slotun son aktif zamanını günceller, bu genellikle bağlantı zaman aşımı yönetimi için kullanılır, bu sayede uygulama yazma işlemi sırasında hangi verilerin gönderildiğini yönetebilir
            continue;
        }
 
        if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) // Eğer write() fonksiyonu EAGAIN veya EWOULDBLOCK hatası döndürürse, bu genellikle gönderilecek daha fazla veri olmadığı anlamına gelir, 
        // bu durumda döngü sonlandırılır, böylece uygulama gereksiz yere kaynak tüketmez ve olay işleme etkin bir şekilde yapılır
            break;
 
        if (n <= 0)
        {
            std::cout << "[PollReactor] write() slot[" << slot_index << "]: " << ((n == 0) ? "closed" : strerror(errno)) << std::endl;
            _close_slot(slot_index);
            return;
        }
    }
 
    if (slot.write_done()) // Eğer yazma işlemi tamamlanmışsa, bu durumda on_write callback'i çağrılır, böylece uygulama yazma işlemi tamamlandığında tepki verebilir ve kaynakları verimli kullanır,
     // eğer slotun durumuna göre okuma veya yazma olayları güncellenir, böylece uygulama gereksiz yere kaynak tüketmez ve olay işleme etkin bir şekilde yapılır
     // slot.write_done(): Yazma işleminin tamamlanıp tamamlanmadığını kontrol eder, bu genellikle write_position'ın writebuffer'daki veri boyuna eşit olup olmadığını
     // kontrol ederek yapılır, eğer yazma işlemi tamamlanmışsa true döndürür, aksi halde false döndürür, bu sayede uygulama yazma işlemi tamamlandığında tepki verebilir ve kaynakları verimli kullanır
    {
        std::cout << "[PollReactor] Yanıt tamamlandı: slot[" << slot_index << "]" << std::endl;
 
        if (_on_write) // Eğer on_write callback'i tanımlıysa, bu callback çağrılır, bu sayede uygulama yazma işlemi tamamlandığında tepki verebilir ve kaynakları verimli kullanır
            _on_write(_cb_ctx, slot); // on_write callback'i çağrılır, bu callback genellikle uygulamanın yazma işlemi tamamlandığında ek işlemler yapması için kullanılır,
            // bu sayede uygulama yazma işlemi tamamlandığında tepki verebilir ve kaynakları verimli kullanır

        if (slot.state == ConnectState_READING) // Eğer slotun durumu okuma ise, bu durumda yazma işlemi tamamlandıktan sonra slotun durumunu okuma olarak bırakır, böylece bu slot üzerinden gelen veriler okunmaya devam edebilir, 
         // bu genellikle recv() çağrısı yaparak okunan verileri slotun readbuffer'ına ekler ve üst katmana iletmek için on_data callback'ini çağırır, böylece uygulama gelen verilere tepki verebilir ve kaynakları verimli kullanır
        {
            slot.readbuffer.clear();
            slot.writebuffer.clear();
            slot.write_position      = 0;
            slot.request_complete    = false;
            _sync_poll_events(slot_index);
        }
        else
            _close_slot(slot_index);
    }
    else
        _pollfds[slot_index].events = POLLOUT; // Eğer yazma işlemi tamamlanmamışsa, bu durumda poll olaylarını POLLOUT olarak ayarlar, böylece poll() fonksiyonu bu fd üzerinden gelen yazma olaylarını izler, 
        // bu genellikle send() çağrısı yaparak writebuffer'daki verileri gönderir, POLLOUT: Veri yazılabilir, bu durum genellikle bu slot üzerinden veri yazılabileceği anlamına gelir, 
        // bu durumda send() çağrısı yaparak writebuffer'daki verileri gönderir, böylece uygulama gelen verilere tepki verebilir ve kaynakları verimli kullanır
}
 
void PollReactor::_close_slot(int slot_index) // Belirli bir slotu kapatır, bu genellikle bağlantıyı kapatır, slotu temizler ve pollfd yapısındaki ilgili alanları sıfırlar, 
// böylece uygulama gereksiz yere kaynak tüketmez ve bağlantı yönetimi etkin bir şekilde yapılır
{
    ConnectionSlot &slot = _slots[slot_index]; // Kapatılacak slotu alır, bu slot genellikle bağlantının durumunu, dosya tanıtıcısını, uzak IP adresini ve portunu, yazma ve okuma buffer'larını tutar, 
    // bu sayede uygulama bağlantı yönetimini etkin bir şekilde yapabilir
 
    if (slot.is_empty()) // Eğer slot zaten boşsa, bu durumda kapatma işlemi yapılmaz, böylece uygulama gereksiz yere kaynak tüketmez ve bağlantı yönetimi etkin bir şekilde yapılır
        return;
 
    std::cout << "[PollReactor] Slot kapatılıyor: [" << slot_index << "] fd=" << slot.fd << " " << slot.remote_ip.c_str() << ":" << (unsigned)slot.remote_port << std::endl;
 
    if (_on_close && !slot.is_server) // Eğer on_close callback'i tanımlıysa ve bu slot bir sunucu soketi değilse, bu callback çağrılır, bu sayede uygulama bağlantı kapatıldığında tepki verebilir ve kaynakları verimli kullanır
        _on_close(_cb_ctx, slot); // on_close callback'i çağrılır, bu callback genellikle uygulamanın bağlantı kapatıldığında ek işlemler yapması için kullanılır, bu sayede uygulama bağlantı kapatıldığında 
        // tepki verebilir ve kaynakları verimli kullanır
 
    SocketBinder::close_fd(slot.fd); // Slotun dosya tanıtıcısını kapatır, bu genellikle SocketBinder sınıfının close_fd fonksiyonu kullanılarak yapılır, bu sayede uygulama gereksiz yere kaynak tüketmez ve 
    // bağlantı yönetimi etkin bir şekilde yapılır
    _pollfds[slot_index].fd     = -1; // Pollfd yapısında bu slotun fd'sini -1 yaparak geçersiz hale getirir, bu sayede poll() fonksiyonu bu fd üzerinden gelen olayları izlemez, 
    // böylece uygulama gereksiz yere kaynak tüketmez ve olay işleme etkin bir şekilde yapılır
    _pollfds[slot_index].events = 0; // Pollfd yapısında bu slotun events alanını sıfırlayarak geçersiz hale getirir, bu sayede poll() fonksiyonu bu fd üzerinden gelen olayları izlemez, 
    // böylece uygulama gereksiz yere kaynak tüketmez ve olay işleme etkin bir şekilde yapılır
    slot.clear(); // Slotu temizler, bu genellikle slotun durumunu boş olarak ayarlamak, dosya tanıtıcısını sıfırlamak, uzak IP adresini ve portunu temizlemek, yazma ve okuma buffer'larını temizlemek gibi işlemleri içerir,
     // bu sayede uygulama gereksiz yere kaynak tüketmez ve bağlantı yönetimi etkin bir şekilde yapılır
 
    while (_slot_count > 0 && _slots[_slot_count - 1].is_empty()) // Eğer slot sayısı sıfırdan büyükse ve son slot boşsa, bu durumda slot sayısını azaltır, böylece uygulama gereksiz yere kaynak tüketmez 
    // ve bağlantı yönetimi etkin bir şekilde yapılır
        _slot_count--; // Slot sayısını azaltır, bu sayede uygulama gereksiz yere kaynak tüketmez ve bağlantı yönetimi etkin bir şekilde yapılır
}
 
void PollReactor::close_slot(int slot_index) // Belirli bir slotu kapatmak için dışarıdan çağrılabilecek bir fonksiyon, bu fonksiyon genellikle slot index'inin geçerli olup olmadığını kontrol eder ve geçerliyse 
//_close_slot fonksiyonunu çağırır
{
    if (slot_index >= 0 && slot_index < MAX_SLOTS) // Eğer slot index'i geçerliyse, bu durumda _close_slot fonksiyonunu çağırır, böylece uygulama gereksiz yere kaynak tüketmez ve bağlantı yönetimi etkin bir şekilde yapılır
        _close_slot(slot_index); // _close_slot fonksiyonunu çağırır, bu fonksiyon genellikle bağlantıyı kapatır, slotu temizler ve pollfd yapısındaki ilgili alanları sıfırlar, 
        //böylece uygulama gereksiz yere kaynak tüketmez ve bağlantı yönetimi etkin bir şekilde yapılır
     // _close_slot(slot_index): Belirli bir slotu kapatır, bu genellikle bağlantıyı kapatır, slotu temizler ve pollfd yapısındaki ilgili alanları sıfırlar, böylece uygulama gereksiz yere kaynak tüketmez 
     //ve bağlantı yönetimi etkin bir şekilde yapılır
}
 
void PollReactor::queue_response(int slot_index, const std::string &data) // Belirli bir slot için yanıt verilerini kuyruğa ekler, bu fonksiyon genellikle slot index'inin geçerli olup olmadığını kontrol eder, 
// slotun boş olmadığını ve dosya tanıtıcısının geçerli olduğunu kontrol eder,
// eğer tüm kontroller geçerse, slotun writebuffer'ına yanıt verilerini ekler ve pollfd yapısındaki events alanını POLLOUT olarak ayarlar, 
// böylece poll() fonksiyonu bu fd üzerinden gelen yazma olaylarını izler, bu genellikle send() çağrısı yaparak writebuffer'daki verileri gönderir, böylece uygulama gelen verilere tepki verebilir ve kaynakları verimli kullanır
{
    if (slot_index < 0 || slot_index >= MAX_SLOTS) // Eğer slot index'i geçerli değilse, bu durumda fonksiyon hiçbir işlem yapmaz ve döner, böylece uygulama gereksiz yere kaynak 
    // tüketmez ve bağlantı yönetimi etkin bir şekilde yapılır
        return;
 
    ConnectionSlot &slot = _slots[slot_index]; // Yanıt verilerini kuyruğa eklemek için slotu alır, bu slot genellikle bağlantının durumunu, dosya tanıtıcısını, uzak IP adresini ve portunu, yazma ve okuma buffer'larını tutar
    if (slot.is_empty() || slot.fd < 0) // Eğer slot boşsa veya dosya tanıtıcısı geçerli değilse, bu durumda fonksiyon hiçbir işlem yapmaz ve döner, 
    // böylece uygulama gereksiz yere kaynak tüketmez ve bağlantı yönetimi etkin bir şekilde yapılır
        return;
 
    slot.queue_response(data); // Slotun writebuffer'ına yanıt verilerini ekler, bu fonksiyon genellikle writebuffer'a data'yı ekler, bu sayede uygulama yazma işlemi sırasında hangi verilerin gönderileceğini yönetebilir
     // slot.queue_response(data): Slotun writebuffer'ına yanıt verilerini ekler, bu fonksiyon genellikle writebuffer'a data'yı ekler, bu sayede uygulama yazma işlemi sırasında hangi verilerin gönderileceğini yönetebilir
    _pollfds[slot_index].events = POLLOUT; // Pollfd yapısında bu slotun events alanını POLLOUT olarak ayarlar, böylece poll() fonksiyonu bu fd üzerinden gelen yazma olaylarını izler, bu genellikle 
    // send() çağrısı yaparak writebuffer'daki verileri gönderir, 
     //böylece uygulama gelen verilere tepki verebilir ve kaynakları verimli kullanır
     // POLLOUT: Veri yazılabilir, bu durum genellikle bu slot üzerinden veri yazılabileceği anlamına gelir, bu durumda send() çağrısı yaparak writebuffer'daki verileri gönderir, 
     // böylece uygulama gelen verilere tepki verebilir ve kaynakları verimli kullanır
}
 
void PollReactor::queue_response(int slot_index, const std::vector<char>& data) // Belirli bir slot için yanıt verilerini kuyruğa ekler, bu fonksiyon genellikle slot index'inin geçerli olup olmadığını kontrol eder, 
//slotun boş olmadığını ve dosya tanıtıcısının geçerli olduğunu kontrol eder
{
    if (slot_index < 0 || slot_index >= MAX_SLOTS)
        return;
 
    ConnectionSlot &slot = _slots[slot_index]; // Yanıt verilerini kuyruğa eklemek için slotu alır, bu slot genellikle bağlantının durumunu, dosya tanıtıcısını, uzak IP adresini ve portunu, yazma ve okuma buffer'larını tutar
    if (slot.is_empty() || slot.fd < 0)
        return;
 
    slot.queue_response(data);
    _pollfds[slot_index].events = POLLOUT;
}
 
int PollReactor::_find_empty_slot() const // Boş bir slot bulmaya çalışır, eğer mevcut slot sayısı MAX_SLOTS'tan küçükse, mevcut slot sayısını döndürür, bu genellikle yeni bir slotun kullanılmaya başlandığı anlamına gelir,
// eğer mevcut slot sayısı MAX_SLOTS'a eşitse, tüm slotları kontrol eder ve boş olan ilk slotun index'ini döndürür, eğer tüm slotlar doluysa -1 döndürür, böylece uygulama yeni bağlantılar için uygun slotları yönetebilir
{
    if (_slot_count < MAX_SLOTS) // Eğer mevcut slot sayısı MAX_SLOTS'tan küçükse, bu genellikle yeni bir slotun kullanılmaya başlandığı anlamına gelir, bu durumda mevcut slot sayısını döndürür,
    // böylece uygulama yeni bağlantılar için uygun slotları yönetebilir
        return _slot_count;
 
    for (int i = 0; i < MAX_SLOTS; ++i) // Eğer mevcut slot sayısı MAX_SLOTS'a eşitse, tüm slotları kontrol eder ve boş olan ilk slotun index'ini döndürür, eğer tüm slotlar doluysa -1 döndürür, 
    // böylece uygulama yeni bağlantılar için uygun slotları yönetebilir
    {
        if (_slots[i].is_empty()) // Eğer slot boşsa, bu durumda bu slotun index'ini döndürür, böylece uygulama yeni bağlantılar için uygun slotları yönetebilir
            return i;
    }
 
    return (-1); // Tüm slotlar dolu
}
 
void PollReactor::_sync_poll_events(int slot_index) // Belirli bir slotun poll olaylarını günceller, bu genellikle slotun durumuna göre pollfd yapısındaki events alanını günceller, 
// eğer slot yazma işlemi yapıyorsa ve yazma işlemi tamamlanmamışsa POLLOUT ekler,
// aksi halde POLLOUT'u kaldırır, böylece uygulama gereksiz yere kaynak tüketmez ve olay işleme etkin bir şekilde yapılır
{
    ConnectionSlot &slot = _slots[slot_index];
 
    if (slot.is_empty())
    {
        _pollfds[slot_index].events = 0; // Eğer slot boşsa, bu durumda pollfd yapısında bu slotun events alanını sıfırlayarak geçersiz hale getirir, böylece poll() fonksiyonu bu fd üzerinden gelen olayları izlemez
        return;
    }
 
    if (slot.is_writing() && !slot.write_done()) // Eğer slot yazma işlemi yapıyorsa ve yazma işlemi tamamlanmamışsa, bu durumda pollfd yapısında bu slotun events alanını POLLOUT olarak ayarlar, 
    // böylece poll() fonksiyonu bu fd üzerinden gelen yazma olaylarını izler
        _pollfds[slot_index].events = POLLOUT;
    else
        _pollfds[slot_index].events = POLLIN;
}
 
void PollReactor::_sweep_timeouts() // Tüm slotları kontrol eder ve zaman aşımına uğramış olanları kapatır, bu genellikle slotun timed_out() fonksiyonunu kullanarak yapılır, 
// eğer bir slot zaman aşımına uğramışsa, bu slot kapatılır, böylece uygulama gereksiz yere kaynak tüketmez ve bağlantı yönetimi etkin bir şekilde yapılır
{
    int closed;
 
    closed = 0;
    for (int i = 0; i < _slot_count; ++i) // Tüm slotları kontrol eder, bu genellikle mevcut slot sayısı kadar döngü yaparak yapılır, bu sayede uygulama gereksiz yere kaynak tüketmez ve bağlantı yönetimi etkin bir şekilde yapılır
    {
        ConnectionSlot &slot = _slots[i];
 
        if (slot.is_empty() || slot.is_server) // Eğer slot boşsa veya bir sunucu soketi ise, bu slot zaman aşımına uğramış olarak değerlendirilmez, 
        // bu durumda bu slotu atlar ve bir sonraki slotu kontrol eder, böylece uygulama gereksiz yere kaynak tüketmez ve bağlantı yönetimi etkin bir şekilde yapılır
            continue;
 
        if (slot.timed_out()) // Eğer slotun timed_out() fonksiyonu true döndürürse, bu genellikle bu slotun zaman aşımına uğradığı anlamına gelir, 
        // bu durumda bu slot kapatılır, böylece uygulama gereksiz yere kaynak tüketmez ve bağlantı yönetimi etkin bir şekilde yapılır
        {
            std::cout << "[PollReactor] Timeout: slot[" << i << "] fd=" << slot.fd << " " << slot.remote_ip.c_str() << ":" << (unsigned)slot.remote_port << std::endl;
            _close_slot(i);
            closed++;
        }
    }
    if (closed > 0)
        std::cout << "[PollReactor] Sweep: " << closed << " bağlantı kapatıldı" << std::endl;
}
 
int PollReactor::active_connections() const // Aktif bağlantı sayısını döndürür, bu genellikle tüm slotları kontrol ederek boş olmayan ve sunucu soketi olmayan slotların sayısını sayarak yapılır, 
// bu sayede uygulama mevcut aktif bağlantı sayısını yönetebilir
{
    int count;

    count = 0;
    for (int i = 0; i < _slot_count; ++i) // Tüm slotları kontrol eder, bu genellikle mevcut slot sayısı kadar döngü yaparak yapılır, bu sayede uygulama mevcut aktif bağlantı sayısını yönetebilir
    {
        if (!_slots[i].is_empty() && !_slots[i].is_server) // Eğer slot boş değilse ve bir sunucu soketi değilse, bu durumda bu slot aktif bir bağlantıyı temsil eder, bu durumda sayacı artırır, 
        // böylece uygulama mevcut aktif bağlantı sayısını yönetebilir
            count++;
    }
    return (count);
}
 
bool PollReactor::is_server_fd(int fd) const // Belirli bir dosya tanıtıcısının sunucu soketi olup olmadığını kontrol eder, 
// bu genellikle tüm slotları kontrol ederek bu fd'ye sahip bir sunucu soketi olup olmadığını kontrol ederek yapılır,
// eğer bu fd'ye sahip bir sunucu soketi bulunursa true döndürür, aksi halde false döndürür, böylece uygulama gelen bağlantıların hangi fd üzerinden geldiğini yönetebilir
{
    for (int i = 0; i < _server_count; ++i) // Tüm sunucu soketlerini kontrol eder, bu genellikle mevcut sunucu sayısı kadar döngü yaparak yapılır, bu sayede uygulama gelen bağlantıların hangi fd üzerinden geldiğini yönetebilir
    {
        if (_servers[i].fd == fd) // Eğer bu fd'ye sahip bir sunucu soketi bulunursa, bu durumda true döndürür, böylece uygulama gelen bağlantıların hangi fd üzerinden geldiğini yönetebilir
            return true;
    }
    return (false);
}

bool PollReactor::watch_fd(int fd, short events, FdCallback callback, void *ctx)
{
    int i;

    if (fd < 0 || callback == NULL)
        return false;

    i = 0;
    while (i < MAX_EXTERNAL_FDS)
    {
        if (_externalFds[i].fd == fd)
        {
            _externalFds[i].events = events;
            _externalFds[i].callback = callback;
            _externalFds[i].ctx = ctx;
            return true;
        }
        i++;
    }

    i = 0;
    while (i < MAX_EXTERNAL_FDS)
    {
        if (_externalFds[i].fd == -1)
        {
            _externalFds[i].fd = fd;
            _externalFds[i].events = events;
            _externalFds[i].callback = callback;
            _externalFds[i].ctx = ctx;
            return true;
        }
        i++;
    }

    return false;
}

void PollReactor::update_fd(int fd, short events)
{
    int i;

    i = 0;
    while (i < MAX_EXTERNAL_FDS)
    {
        if (_externalFds[i].fd == fd)
        {
            _externalFds[i].events = events;
            return;
        }
        i++;
    }
}

void PollReactor::unwatch_fd(int fd)
{
    int i;

    i = 0;
    while (i < MAX_EXTERNAL_FDS)
    {
        if (_externalFds[i].fd == fd)
        {
            _externalFds[i].fd = -1;
            _externalFds[i].events = 0;
            _externalFds[i].callback = NULL;
            _externalFds[i].ctx = NULL;
            return;
        }
        i++;
    }
}
