#ifndef NET_SUPPORT_H
#define NET_SUPPORT_H


#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <errno.h>


FILE * get_net(const char *hostname, int port);


FILE * get_net(const char *hostname, int port)
{
    struct sockaddr_in address = {.sin_family = AF_INET, .sin_port = htons(port)};
    if (-1 == inet_aton(hostname, &address.sin_addr))
    {
        fprintf(stderr, "inet_aton() error\n");
        return NULL;
    }

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == -1)
    {
        fprintf(stderr, "socket() error\n");
        return NULL;
    }

    if (connect(fd, (struct sockaddr *) &address, sizeof (address)) == -1)
    {
        fprintf(stderr, "connect() error: %s:%d\n", hostname, port);
        return NULL;
    }

    FILE *input = fdopen(fd, "rb");
    if (input == NULL)
    {
        fprintf(stderr, "fdopen() error\n");
    }

    return input;
}


#endif /* NET_SUPPORT_H */
