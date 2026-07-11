#ifndef NETTYPES_HPP
#define NETTYPES_HPP

#include <string>
#include <vector>
#include <stdint.h>
#include <sys/types.h>

static const int MAX_SLOTS         = 1024;
static const int READ_CHUNK        = 256 * 1024;
static const int POLL_TIMEOUT_MS   = 5000;
static const int CONNECT_IDLE_TIMEOUT = 60;
static const int MAX_SERVERS       = 8;


enum ConnectionState
{
    ConnectState_EMPTY   = 0,
    ConnectState_READING = 1,
    ConnectState_WRITING = 2,
    ConnectState_CLOSING = 3
};


struct ServerBind
{
    std::string host;
    uint16_t    port;
    int         fd;
 
    ServerBind() : port(0), fd(-1) {}
    ServerBind(const std::string &hst, uint16_t prt): host(hst), port(prt), fd(-1) {}
};

inline bool fd_valid(int fd)
{
    return fd > 0; 
}

#endif
