#ifndef POLLREACTOR_HPP
#define POLLREACTOR_HPP

#include "Nettypes.hpp"
#include "Connectionslot.hpp"
#include "Socketbinder.hpp"
#include "Signals.hpp"
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
#include <sstream>
#include "../utils/Utils.hpp"

typedef void (*DataCallback)(void *ctx, ConnectionSlot &slot);
typedef void (*FdCallback)(void *ctx, int fd, short revents);
typedef bool (*TimeoutCallback)(void *ctx, ConnectionSlot &slot);

class PollReactor
{
    public:
    
        PollReactor();
        ~PollReactor();
        bool add_server(const std::string &host, uint16_t port);

        void set_callbacks
        (
            DataCallback on_data,
            DataCallback on_write,
            DataCallback on_close,
            void*        context,
            TimeoutCallback on_timeout = NULL
        );

        void run();
        void request_shutdown();
        void queue_response(int slot_index, const std::string &data);
        void queue_response(int slot_index, const std::vector<char> &data);
        void queue_interim_response(int slot_index, const std::string &data);
        bool queue_response_file(int slot_index, const std::string &headers,
                                 const std::string &filePath, size_t offset,
                                 size_t length, bool unlinkAfterSend);
        void close_slot(int slot_index);
        int  active_connections() const;
        bool is_server_fd(int fd) const;
    
        bool watch_fd(int fd, short events, FdCallback callback, void *ctx);
        void update_fd(int fd, short events);
        void unwatch_fd(int fd);

    private:
        void _accept_client(int server_slot_index);
        void _read_slot(int slot_index);
        void _write_slot(int slot_index);
        bool _write_response_file(ConnectionSlot &slot);
        void _close_slot(int slot_index);
        int  _find_empty_slot() const;
        void _sweep_timeouts();
        void _sync_poll_events(int slot_index);
        
        ConnectionSlot _slots[MAX_SLOTS];
        struct pollfd  _pollfds[MAX_SLOTS];
        int            _slot_count;
        ServerBind     _servers[MAX_SERVERS];
        int            _server_count;
        DataCallback   _on_data;
        DataCallback   _on_write;
        DataCallback   _on_close;
        TimeoutCallback _on_timeout;
        void*          _cb_ctx;
        bool           _running;
        int            _sweep_counter;
        static const int SWEEP_INTERVAL = 100;
        PollReactor(const PollReactor&);
        PollReactor& operator=(const PollReactor&);

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
