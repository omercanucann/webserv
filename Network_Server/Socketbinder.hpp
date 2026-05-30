#ifndef SOCKETBINDER_HPP
#define SOCKETBINDER_HPP

#include "Nettypes.hpp"
#include <string>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <arpa/inet.h>
#include <sstream>
#include <netdb.h>
#include <fcntl.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#include <cstdio>
#include <iostream>
#include "../utils/Utils.hpp"

class SocketBinder
{
    public:
        static int  open_listener(const std::string& host, uint16_t port); //ne işe yarar? : Belirtilen host ve port üzerinde bir dinleyici soketi açar ve bu soketin dosya tanıtıcısını döndürür. Eğer başarısız olursa, -1 döndürür.  
        static bool set_nonblocking(int fd); // ne işe yarar? : Verilen dosya tanıtıcısını (fd) non-blocking moda geçirir. Başarılı olursa true, başarısız olursa false döndürür.
        static void close_fd(int& fd); // ne işe yarar? : Verilen dosya tanıtıcısını (fd) kapatır ve fd'yi -1 yapar. Bu, kaynak sızıntılarını önlemek için önemlidir.
    
    private:
        SocketBinder(); // Constructor'u private yaparak sınıfın örneklendirilememesini sağlıyoruz.
        SocketBinder(const SocketBinder&); // Copy constructor'u private yaparak sınıfın kopyalanamamasını sağlıyoruz.
        SocketBinder& operator=(const SocketBinder&); // Assignment operator'u private yaparak sınıfın atanamamasını sağlıyoruz.
};

#endif