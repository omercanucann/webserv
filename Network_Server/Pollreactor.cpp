#include "Pollreactor.hpp"
#include <fcntl.h>
#include <cstring>

PollReactor::PollReactor(): _slot_count(0), _server_count(0), _on_data(NULL), _on_write(NULL), _on_close(NULL), _on_timeout(NULL), _cb_ctx(NULL), _running(false), _sweep_counter(0)
{
    std::memset(_pollfds, 0, sizeof(_pollfds));
    for (int i = 0; i < MAX_SLOTS; ++i)
        _pollfds[i].fd = -1;
 
    for (int i = 0; i < MAX_SERVERS; ++i)
        _servers[i].fd = -1;
}

PollReactor::~PollReactor()
{
    for (int i = 0; i < MAX_SLOTS; ++i)
    {
        if (_slots[i].fd > 0)
            SocketBinder::close_fd(_slots[i].fd);
    }
}

bool PollReactor::add_server(const std::string &host, uint16_t port)
{
    int listen_fd;
    int slot_index;

    if (_server_count >= MAX_SERVERS)
    {
        std::cout << "[PollReactor] The maximum number of servers has been reached. (" << MAX_SERVERS << ")" << std::endl;
        return (false);
    }
 
    listen_fd = SocketBinder::open_listener(host, port); 
    if (listen_fd < 0)
        return (false);
 
    _servers[_server_count].host = host;
    _servers[_server_count].port = port;
    _servers[_server_count].fd   = listen_fd;
    _server_count++;
 
    slot_index = _find_empty_slot();
    if (slot_index < 0)
    {
        std::cout << "[PollReactor] Slot not found for server fd=" << listen_fd << std::endl;
        SocketBinder::close_fd(listen_fd);
        return (false);
    }
 
    _slots[slot_index].state       = ConnectState_READING;
    _slots[slot_index].fd          = listen_fd;
    _slots[slot_index].is_server   = true;
    _slots[slot_index].last_active = time(NULL);
 
    _pollfds[slot_index].fd     = listen_fd;
    _pollfds[slot_index].events = POLLIN;
 
    if (slot_index >= _slot_count)
        _slot_count = slot_index + 1;
 
    std::cout << "[PollReactor] Server slot[" << slot_index << "] fd=" << listen_fd << " " << host.c_str() << ":" << (unsigned)port << std::endl;
 
    return (true);
}

void PollReactor::set_callbacks(DataCallback on_data, DataCallback on_write,
                                DataCallback on_close, void *context,
                                TimeoutCallback on_timeout)
{
    _on_data  = on_data;
    _on_write = on_write;
    _on_close = on_close;
    _on_timeout = on_timeout;
    _cb_ctx   = context;
}

void PollReactor::run()
{
    struct pollfd pollList[MAX_SLOTS + MAX_EXTERNAL_FDS];
    int externalMap[MAX_SLOTS + MAX_EXTERNAL_FDS];
    int pollCount;
    int i;
    int processed;
    int n_ready;
    short rev;

    if (_server_count == 0)
    {
        std::cout << "[PollReactor] No servers added, exiting." << std::endl;
        return;
    }
 
    _running = true;
    std::cout << "[PollReactor] Event loop started (" << _server_count << " servers)" << std::endl;
 
    while (_running && !SignalHandler::shutdown_requested()) 
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

        if (n_ready < 0) 
        {
            if (errno == EINTR) 
                continue;
            std::cerr << "[PollReactor] poll: " << strerror(errno) << std::endl;
            break;
        }
 
        if (n_ready == 0)
        {
            _sweep_timeouts();
            continue;
        }
 
        processed = 0;
        for (int i = 0; i < _slot_count && processed < n_ready; ++i)
        {
            rev = _pollfds[i].revents; 
            if (rev == 0)
                continue;
            processed++; 
            if (rev & (POLLHUP | POLLERR | POLLNVAL)) 
            {
                if (!_slots[i].is_server)
                {
                    std::cout << "[PollReactor] Slot[" << i << "] error/close (rev=0x" << std::hex << (unsigned)rev << std::dec << ")" << std::endl;
                    _close_slot(i);
                }
                continue;
            }
 
            if (rev & POLLIN)
            {
                if (_slots[i].is_server)
                    _accept_client(i);
                else
                    _read_slot(i);
            }

            if (rev & POLLOUT)
            {
                if (!_slots[i].is_server)
                    _write_slot(i);
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
        
        _sweep_counter++;
        if (_sweep_counter >= SWEEP_INTERVAL)
        {
            _sweep_timeouts();
            _sweep_counter = 0; 
        }
    }
    std::cout << "[PollReactor] The cycle has ended." << std::endl;
}
 
void PollReactor::request_shutdown()
{
    _running = false;
}
 
void PollReactor::_accept_client(int server_slot_index)
{
    struct sockaddr_in client_addr;
    socklen_t          addr_len;
    int server_fd;
    int slot_index;
 
    addr_len = sizeof(client_addr);
    server_fd = _slots[server_slot_index].fd;
 
    while (true)
    {
        int client_fd;
 
        client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &addr_len); 
        if (client_fd < 0)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                break;
            std::cerr << "[PollReactor] accept: " << strerror(errno) << std::endl;
            break;
        }
 
        if (!SocketBinder::set_nonblocking(client_fd))
        {
            SocketBinder::close_fd(client_fd);
            continue;
        }

        slot_index = _find_empty_slot();
        if (slot_index < 0)
        {
            std::cout << "[PollReactor] Slot full, connection rejected." << std::endl;
            close(client_fd); 
            continue;
        }

        unsigned char *ip_bytes;
        std::ostringstream ip_stream;

        ip_bytes = reinterpret_cast<unsigned char *>(&client_addr.sin_addr.s_addr);

        ip_stream << static_cast<unsigned int>(ip_bytes[0]) << "."
                << static_cast<unsigned int>(ip_bytes[1]) << "."
                << static_cast<unsigned int>(ip_bytes[2]) << "."
                << static_cast<unsigned int>(ip_bytes[3]);

        ConnectionSlot &slot     = _slots[slot_index];
        slot.state               = ConnectState_READING;
        slot.fd                  = client_fd;
        slot.is_server           = false;
        slot.remote_ip           = ip_stream.str();
        slot.remote_port         = ntohs(client_addr.sin_port);
        slot.self_index            = slot_index;
        slot.origin_server_fd    = server_fd;
        slot.request_complete    = false;
        slot.continue_sent       = false;
        slot.readbuffer.clear();
        slot.writebuffer.clear();
        slot.write_position      = 0;
        slot.touch();
 
        _pollfds[slot_index].fd      = client_fd;
        _pollfds[slot_index].events  = POLLIN;
        _pollfds[slot_index].revents = 0;
 
        if (slot_index >= _slot_count)
            _slot_count = slot_index + 1;
 
        std::cout << "[PollReactor] New link: slot[" << slot_index << "] fd=" << client_fd << " " << slot.remote_ip << ":" << (unsigned)ntohs(client_addr.sin_port) << std::endl;
    }
}
 
void PollReactor::_read_slot(int slot_index)
{
    char buf[READ_CHUNK];
    ConnectionSlot &slot = _slots[slot_index];

    slot.self_index = slot_index;
    ssize_t n = read(slot.fd, buf, sizeof(buf));

    if (n > 0)
    {
        slot.readbuffer.insert(slot.readbuffer.end(), buf, buf + n);
        slot.touch();

        if (_on_data)
            _on_data(_cb_ctx, slot);
        return;
    }

    if (n == 0)
    {
        _close_slot(slot_index);
        return;
    }

    std::cout << "[PollReactor] read() failed: slot[" << slot_index << "]" << std::endl;
    _close_slot(slot_index);
}
 
void PollReactor::_write_slot(int slot_index)
{
    ConnectionSlot &slot = _slots[slot_index];

    if (slot.interim_write_pending())
    {
        const char *data = slot.interim_writebuffer.data()
            + slot.interim_write_position;
        size_t remain = slot.pending_interim_write();
        ssize_t n = write(slot.fd, data, remain);

        if (n > 0)
        {
            slot.interim_write_position += static_cast<size_t>(n);
            slot.touch();
        }
        else
        {
            std::cout << "[PollReactor] interim write() failed: slot["
                      << slot_index << "]" << std::endl;
            _close_slot(slot_index);
            return;
        }

        if (!slot.interim_write_pending())
            slot.clear_interim_response();
        _sync_poll_events(slot_index);
        return;
    }

    if (slot.write_done())
    {
        _sync_poll_events(slot_index);
        return;
    }

    if (slot.write_position < slot.writebuffer.size())
    {
        const char *data = slot.writebuffer.data() + slot.write_position;
        size_t remain = slot.pending_write();
        ssize_t n = write(slot.fd, data, remain);

        if (n > 0)
        {
            slot.write_position += static_cast<size_t>(n);
            slot.touch();
        }
        else
        {
            std::cout << "[PollReactor] write() failed: slot[" << slot_index << "]" << std::endl;
            _close_slot(slot_index);
            return;
        }
    }
    else if (!_write_response_file(slot))
    {
        std::cout << "[PollReactor] file write failed: slot[" << slot_index << "]" << std::endl;
        _close_slot(slot_index);
        return;
    }

    if (slot.write_done())
    {
        std::cout << "[PollReactor] The answer is complete: slot[" << slot_index << "]" << std::endl;
        slot.clear_response_file();

        if (_on_write)
            _on_write(_cb_ctx, slot);

        if (slot.state == ConnectState_READING)
        {
            slot.readbuffer.clear();
            slot.writebuffer.clear();
            slot.write_position = 0;
            slot.request_complete = false;
            _sync_poll_events(slot_index);
        }
        else
            _close_slot(slot_index);
    }
    else
        _pollfds[slot_index].events = POLLOUT;
}

bool PollReactor::_write_response_file(ConnectionSlot &slot)
{
    ResponseFileState *file;
    ssize_t n;
    size_t chunkSize;

    file = slot.response_file;
    if (file == NULL || file->fd < 0)
        return true;

    if (file->buffer_position >= file->buffer.size())
    {
        file->buffer.clear();
        file->buffer_position = 0;

        if (file->remaining == 0)
            return true;

        chunkSize = file->remaining;
        if (chunkSize > static_cast<size_t>(READ_CHUNK))
            chunkSize = static_cast<size_t>(READ_CHUNK);

        file->buffer.resize(chunkSize);
        n = read(file->fd, &file->buffer[0], chunkSize);
        if (n > 0)
        {
            file->buffer.resize(static_cast<size_t>(n));
            file->remaining -= static_cast<size_t>(n);
        }
        else
        {
            file->buffer.clear();
            return false;
        }
    }

    if (file->buffer_position < file->buffer.size())
    {
        const char *data = &file->buffer[0] + file->buffer_position;
        size_t remain = file->buffer.size() - file->buffer_position;

        n = write(slot.fd, data, remain);
        if (n > 0)
        {
            file->buffer_position += static_cast<size_t>(n);
            slot.touch();
            if (file->buffer_position >= file->buffer.size())
            {
                file->buffer.clear();
                file->buffer_position = 0;
            }
            return true;
        }
        return false;
    }

    return true;
}
 
void PollReactor::_close_slot(int slot_index)
{
    ConnectionSlot &slot = _slots[slot_index];
 
    if (slot.is_empty())
        return;
 
    if (!slot.is_server)
        slot.self_index = slot_index;

    std::cout << "[PollReactor] Closing slot: [" << slot_index << "] fd=" << slot.fd << " " << slot.remote_ip.c_str() << ":" << (unsigned)slot.remote_port << std::endl;
 
    if (_on_close && !slot.is_server)
        _on_close(_cb_ctx, slot);
 
    SocketBinder::close_fd(slot.fd);
    _pollfds[slot_index].fd     = -1;
    _pollfds[slot_index].events = 0;
    slot.clear();
 
    while (_slot_count > 0 && _slots[_slot_count - 1].is_empty())
        _slot_count--;
}
 
void PollReactor::close_slot(int slot_index)
{
    if (slot_index >= 0 && slot_index < MAX_SLOTS)
        _close_slot(slot_index);
}
 
void PollReactor::queue_response(int slot_index, const std::string &data)
{
    if (slot_index < 0 || slot_index >= MAX_SLOTS)
        return;
 
    ConnectionSlot &slot = _slots[slot_index];
    if (slot.is_empty() || slot.fd < 0 || slot.is_server)
        return;
 
    slot.queue_response(data);
    _pollfds[slot_index].events = POLLOUT;
}
 
void PollReactor::queue_response(int slot_index, const std::vector<char>& data)
{
    if (slot_index < 0 || slot_index >= MAX_SLOTS)
        return;
 
    ConnectionSlot &slot = _slots[slot_index];
    if (slot.is_empty() || slot.fd < 0 || slot.is_server)
        return;
 
    slot.queue_response(data);
    _pollfds[slot_index].events = POLLOUT;
}

void PollReactor::queue_interim_response(int slot_index, const std::string &data)
{
    if (slot_index < 0 || slot_index >= MAX_SLOTS)
        return;

    ConnectionSlot &slot = _slots[slot_index];
    if (slot.is_empty() || slot.fd < 0 || slot.is_server)
        return;

    slot.queue_interim_response(data);
    _pollfds[slot_index].events = POLLOUT;
}

bool PollReactor::queue_response_file(int slot_index, const std::string &headers,
                                      const std::string &filePath, size_t offset,
                                      size_t length, bool unlinkAfterSend)
{
    int fileFd;

    if (slot_index < 0 || slot_index >= MAX_SLOTS)
        return false;

    ConnectionSlot &slot = _slots[slot_index];
    if (slot.is_empty() || slot.fd < 0 || slot.is_server)
        return false;

    fileFd = open(filePath.c_str(), O_RDONLY);
    if (fileFd < 0)
        return false;

    if (lseek(fileFd, static_cast<off_t>(offset), SEEK_SET) == static_cast<off_t>(-1))
    {
        close(fileFd);
        return false;
    }

    slot.clear_response_file();
    slot.writebuffer.assign(headers.begin(), headers.end());
    slot.write_position = 0;
    slot.state = ConnectState_WRITING;
    slot.attach_response_file(fileFd, filePath, length, unlinkAfterSend);
    _pollfds[slot_index].events = POLLOUT;
    return true;
}
 
int PollReactor::_find_empty_slot() const
{
    if (_slot_count < MAX_SLOTS)
        return _slot_count;
 
    for (int i = 0; i < MAX_SLOTS; ++i)
    {
        if (_slots[i].is_empty())
            return i;
    }
 
    return (-1);
}
 
void PollReactor::_sync_poll_events(int slot_index)
{
    ConnectionSlot &slot = _slots[slot_index];
 
    if (slot.is_empty())
    {
        _pollfds[slot_index].events = 0;
        return;
    }
 
    if (slot.interim_write_pending())
        _pollfds[slot_index].events = POLLOUT;
    else if (slot.is_writing() && !slot.write_done())
        _pollfds[slot_index].events = POLLOUT;
    else
        _pollfds[slot_index].events = POLLIN;
}
 
void PollReactor::_sweep_timeouts()
{
    int closed;
 
    closed = 0;
    for (int i = 0; i < _slot_count; ++i)
    {
        ConnectionSlot &slot = _slots[i];
 
        if (slot.is_empty() || slot.is_server)
            continue;
 
        if (slot.timed_out())
        {
            if (_on_timeout != NULL && _on_timeout(_cb_ctx, slot))
            {
                slot.touch();
                _sync_poll_events(i);
                continue;
            }
            std::cout << "[PollReactor] Timeout: slot[" << i << "] fd=" << slot.fd << " " << slot.remote_ip.c_str() << ":" << (unsigned)slot.remote_port << std::endl;
            _close_slot(i);
            closed++;
        }
    }
    if (closed > 0)
        std::cout << "[PollReactor] Sweep: " << closed << " connections closed" << std::endl;
}
 
int PollReactor::active_connections() const
{
    int count;

    count = 0;
    for (int i = 0; i < _slot_count; ++i)
    {
        if (!_slots[i].is_empty() && !_slots[i].is_server)
            count++;
    }
    return (count);
}
 
bool PollReactor::is_server_fd(int fd) const
{
    for (int i = 0; i < _server_count; ++i)
    {
        if (_servers[i].fd == fd)
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
