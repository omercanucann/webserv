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

class SocketBinder
{
    public:
        static int  open_listener(const std::string& host, uint16_t port);
        static bool set_nonblocking(int fd);
        static void close_fd(int& fd);
    
    private:
        SocketBinder();
        SocketBinder(const SocketBinder&);
        SocketBinder& operator=(const SocketBinder&);
};

#endif
