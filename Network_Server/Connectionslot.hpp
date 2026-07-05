#ifndef CONNECTIONSLOT_HPP
#define CONNECTIONSLOT_HPP

#include "Nettypes.hpp"
#include <string>
#include <ctime>
#include <cstring>

class ConnectionSlot
{
public:
    ConnectionState state;
    int             fd;
    bool            is_server;
 
    std::string     remote_ip;
    uint16_t        remote_port;
 
    std::vector<char> readbuffer;
    bool              request_complete;

    std::vector<char> writebuffer;
    size_t            write_position;
 
    time_t  last_active;
 
    int self_index;
    int origin_server_fd;

 
    ConnectionSlot()
        : state(ConnectState_EMPTY)
        , fd(-1)
        , is_server(false)
        , remote_port(0)
        , request_complete(false)
        , write_position(0)
        , last_active(0)
        , self_index(-1)
        , origin_server_fd(-1)
    {}
 
    void clear()
    {
        state            = ConnectState_EMPTY;
        fd               = -1;
        is_server        = false;
        remote_ip.clear();
        remote_port      = 0;
        self_index         = -1;
        readbuffer.clear();
        request_complete = false;
        writebuffer.clear();
        write_position        = 0;
        last_active      = 0;
        origin_server_fd = -1;
    }
 
    bool is_empty()     const { return state == ConnectState_EMPTY; }
    bool is_reading()   const { return state == ConnectState_READING; }
    bool is_writing()   const { return state == ConnectState_WRITING; }
    bool is_closing()   const { return state == ConnectState_CLOSING; }
    bool write_done()   const { return write_position >= writebuffer.size(); }
 
    size_t pending_write() const
    {
        if (write_position >= writebuffer.size()) return 0;
        return writebuffer.size() - write_position;
    }
 
    bool timed_out() const
    {
        if (last_active == 0) return false;
        return (time(NULL) - last_active) > CONNECT_IDLE_TIMEOUT;
    }
 
    void queue_response(const std::vector<char> &data)
    {
        writebuffer	= data;
        write_position = 0;
        state	= ConnectState_WRITING;
    }
 
    void queue_response(const std::string &data)
    {
        writebuffer.assign(data.begin(), data.end());
        write_position = 0;
        state     = ConnectState_WRITING;
    }
 
    void touch() { last_active = time(NULL); }
};

#endif