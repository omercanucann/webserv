#include "Utils.hpp"

void *ft_memset(void *s, int c, std::size_t n)
{
    unsigned char *ptr;
    unsigned char byte;

    ptr = (unsigned char *)s;
    byte = (unsigned char)c;
    
    while (n > 0)
    {
        *ptr = byte;
        ptr++;
        n--;
    }
    
    return (s);
}
