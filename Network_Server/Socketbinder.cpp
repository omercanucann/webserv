#include "Socketbinder.hpp"
 
int SocketBinder::open_listener(const std::string &host, uint16_t port)
{
    struct addrinfo mark;
    struct addrinfo* result;
    const char* node;
    int fd;
    int reuse;
    int getaddrinformation_err;
    std::string port_string;
    std::ostringstream oss;
 
    result = NULL;
    ft_memset(&mark, 0, sizeof(mark));
    mark.ai_family   = AF_INET;
    mark.ai_socktype = SOCK_STREAM;
    mark.ai_flags    = AI_PASSIVE;
 
    oss << (unsigned)port;
    port_string = oss.str();
    std::cout << (unsigned)port << std::endl;
 
    if (host == "0.0.0.0" || host.empty())
        node = NULL;
    else
        node = host.c_str();
 
    getaddrinformation_err = getaddrinfo(node, port_string.c_str(), &mark, &result);
    if (getaddrinformation_err != 0)
    {
        std::cerr << "[SocketBinder] getaddrinfo(" << host.c_str() << ":" << (unsigned)port << "): " << gai_strerror(getaddrinformation_err) << std::endl;
        return (-1);
    }
 
    fd = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (fd < 0)
    {
        std::cerr << "[SocketBinder] socket: " << strerror(errno) << std::endl;
        freeaddrinfo(result);
        return (-1);
    }
    reuse = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0)
    {
        std::cerr << "[SocketBinder] setsockopt SO_REUSEADDR: " << strerror(errno) << std::endl;
        close(fd);
        freeaddrinfo(result);
        return (-1);
    }
 
    if (!set_nonblocking(fd))
    {
        close(fd);
        freeaddrinfo(result);
        return (-1);
    }

    if (bind(fd, result->ai_addr, result->ai_addrlen) < 0)
    {
        std::cerr << "[SocketBinder] bind: " << strerror(errno) << std::endl;
        close(fd);
        freeaddrinfo(result);
        return (-1);
    }
    freeaddrinfo(result);
    if (listen(fd, SOMAXCONN) < 0)
    {
        std::cerr << "[SocketBinder] listen: " << strerror(errno) << std::endl;
        close(fd);
        return (-1);
    }
 
    std::cout << "[SocketBinder] Listening on " << host.c_str() << ":" << (unsigned)port << " (fd=" << fd << ")" << std::endl;
    return (fd);
}
 
bool SocketBinder::set_nonblocking(int fd)
{
    int flags;

    flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0)
    {
        std::cerr << "[SocketBinder] fcntl F_GETFL: " << strerror(errno) << std::endl;
        return false;
    }
 
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0)
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