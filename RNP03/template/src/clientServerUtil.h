#ifndef _CLIENTSERVERUTIL_H
#define _CLIENTSERVERUTIL_H

#include <sys/socket.h> 

void convertAddressToString(struct sockaddr *addr, char *ip, size_t ipSize, int *port);

int getClientIpAddress(int socketFd, char *ip, size_t ipSize);

#endif