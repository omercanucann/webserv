#include "Socketbinder.hpp"
 
int SocketBinder::open_listener(const std::string &host, uint16_t port)
{
    struct addrinfo mark; // mark nedir ? : getaddrinfo fonksiyonuna parametre olarak verilen bir yapı. 
    //Bu yapı, getaddrinfo'nun hangi tür adresler ve soketler oluşturması gerektiğini belirtmek için kullanılır. Örneğin, IPv4 mü yoksa IPv6 mı kullanılacağı, 
    //TCP mi yoksa UDP mi kullanılacağı gibi bilgileri içerir. Bu yapı doldurularak getaddrinfo'ya iletilir ve ona göre uygun adres bilgileri döndürülür.
    struct addrinfo* result;
    const char* node;
    int fd;
    int reuse;
    int getaddrinformation_err;
    std::string port_string;
    std::ostringstream oss;
 
    result = NULL; // result nedir ? : getaddrinfo fonksiyonunun sonucunu tutacak bir işaretçi. getaddrinfo, verilen host ve port bilgilerine göre uygun adres bilgilerini içeren bir linked list döndürür.
    ft_memset(&mark, 0, sizeof(mark)); // mark yapısını sıfırlamak için kullanılan bir fonksiyon. Bu, mark yapısının tüm alanlarını 0 yapar, böylece getaddrinfo'ya iletilmeden önce temiz bir yapı elde edilir.
    mark.ai_family   = AF_INET;       // IPv4
    mark.ai_socktype = SOCK_STREAM;   // TCP
    mark.ai_flags    = AI_PASSIVE;    // Bağlama için uygun adres
 
    oss << (unsigned)port;
    port_string = oss.str();
    std::cout << (unsigned)port << std::endl;
 
    if (host == "0.0.0.0" || host.empty())
        node = NULL;
    else
        node = host.c_str(); // host.c_str() nedir ? : std::string türündeki host değişkeninin C tarzı string (null-terminated char array) temsilini döndürür. getaddrinfo fonksiyonu C tarzı string beklediği için, 
        // std::string'i c_str() ile dönüştürmek gerekir.
 
    getaddrinformation_err = getaddrinfo(node, port_string.c_str(), &mark, &result); // getaddrinfo fonksiyonu, verilen node (host) ve service (port) bilgilerine göre uygun adres bilgilerini result işaretçisine doldurur. 
    //Eğer başarılı olursa 0 döndürür, başarısız olursa hata kodu döndürür.
    if (getaddrinformation_err != 0) // getaddrinfo başarısız olduysa
    {
        std::cerr << "[SocketBinder] getaddrinfo(" << host.c_str() << ":" << (unsigned)port << "): " << gai_strerror(getaddrinformation_err) << std::endl;
        return (-1);
    }
 
    fd = socket(result->ai_family, result->ai_socktype, result->ai_protocol); // socket fonksiyonu, belirtilen adres ailesi (IPv4), soket türü (TCP) ve protokol bilgilerine göre bir soket oluşturur. 
    //Başarılı olursa yeni soketin dosya tanıtıcısını döndürür, başarısız olursa -1 döndürür.
    if (fd < 0)
    {
        std::cerr << "[SocketBinder] socket: " << strerror(errno) << std::endl;
        freeaddrinfo(result);
        return (-1);
    }
    reuse = 1; // SO_REUSEADDR seçeneği, bir soketin aynı adrese ve porta yeniden bağlanmasına izin verir. Bu, özellikle sunucu uygulamalarında, soketin kapatıldıktan sonra hemen yeniden başlatılabilmesi için önemlidir.
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) // setsockopt fonksiyonu, belirtilen soket üzerinde belirli bir seçenek (bu durumda SO_REUSEADDR) ayarlamak için kullanılır. 
    //Başarılı olursa 0 döndürür, başarısız olursa -1 döndürür.
    {
        std::cerr << "[SocketBinder] setsockopt SO_REUSEADDR: " << strerror(errno) << std::endl;
        close(fd);
        freeaddrinfo(result);
        return (-1);
    }
 
    if (!set_nonblocking(fd)) // Soketi non-blocking moda geçirir. Eğer başarısız olursa, -1 döndürür. nonblocking mod: Soket işlemlerinin (örneğin, accept, recv) hemen tamamlanmaması durumunda, 
    //bu işlemlerin bloklanmadan geri dönmesini sağlar. Bu, sunucu uygulamalarında performansı artırmak için önemlidir.
    {
        close(fd);
        freeaddrinfo(result);
        return (-1);
    }

    if (bind(fd, result->ai_addr, result->ai_addrlen) < 0) // bind fonksiyonu, oluşturulan soketi belirtilen adres ve porta bağlar. Başarılı olursa 0 döndürür, başarısız olursa -1 döndürür.
    {
        std::cerr << "[SocketBinder] bind: " << strerror(errno) << std::endl;
        close(fd);
        freeaddrinfo(result);
        return (-1);
    }
    freeaddrinfo(result); // getaddrinfo tarafından ayrılan bellek alanını serbest bırakır. getaddrinfo fonksiyonu, uygun adres bilgilerini içeren bir linked list döndürür ve bu liste dinamik olarak ayrılmıştır.
    if (listen(fd, SOMAXCONN) < 0) // listen fonksiyonu, belirtilen soketi dinleme moduna geçirir. Bu, sunucu uygulamalarında gelen bağlantıları kabul etmek için gereklidir. Başarılı olursa 0 döndürür, başarısız olursa -1 döndürür.
    //SOMAXCONN, sistem tarafından desteklenen maksimum bağlantı kuyruğu uzunluğunu belirtir. Bu, aynı anda bekleyen bağlantıların sayısını sınırlar.
    {
        std::cerr << "[SocketBinder] listen: " << strerror(errno) << std::endl;
        close(fd);
        return (-1);
    }
 
    std::cout << "[SocketBinder] Listening on " << host.c_str() << ":" << (unsigned)port << " (fd=" << fd << ")" << std::endl;
    return (fd);
}
 
bool SocketBinder::set_nonblocking(int fd) // set_nonblocking fonksiyonu, verilen dosya tanıtıcısını (fd) non-blocking moda geçirir. Başarılı olursa true, başarısız olursa false döndürür. 
// non-blocking mod: Soket işlemlerinin (örneğin, accept, recv) hemen tamamlanmaması durumunda, 
    //bu işlemlerin bloklanmadan geri dönmesini sağlar. Bu, sunucu uygulamalarında performansı artırmak için önemlidir.
{
    int flags;

    flags = fcntl(fd, F_GETFL, 0); // fcntl fonksiyonu, belirtilen dosya tanıtıcısı (fd) üzerinde çeşitli işlemler yapmaya yarar. F_GETFL komutu, dosya tanıtıcısının mevcut durumunu (flags) almak için kullanılır.
    if (flags < 0)
    {
        std::cerr << "[SocketBinder] fcntl F_GETFL: " << strerror(errno) << std::endl;
        return false;
    }
 
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) // fcntl fonksiyonu, belirtilen dosya tanıtıcısı (fd) üzerinde çeşitli işlemler yapmaya yarar. F_SETFL komutu, dosya tanıtıcısının durumunu (flags) ayarlamak için kullanılır. 
    //Bu satır, mevcut flags değerine O_NONBLOCK bayrağını ekleyerek dosya tanıtıcısını non-blocking moda geçirir. Başarılı olursa 0 döndürür, başarısız olursa -1 döndürür.
    {
        std::cerr << "[SocketBinder] fcntl F_SETFL O_NONBLOCK: " << strerror(errno) << std::endl;
        return false;
    }
 
    return (true);
}
 
void SocketBinder::close_fd(int &fd)
{
    if (fd > 0)
    {
        close(fd);
        fd = -1;
    }
}