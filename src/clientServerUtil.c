#include "clientServerUtil.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>

int convertAddressToString(struct sockaddr *addr, char *ip, size_t ipSize, int *port)
{
    int returnValue = EXIT_SUCCESS;
    int af = 0;
    struct sockaddr_in *ipv4 = NULL;
    struct sockaddr_in6 *ipv6 = NULL;
    if (addr->sa_family == AF_INET)
    {
        af = AF_INET;
        ipv4 = (struct sockaddr_in *)addr;
        inet_ntop(AF_INET, &(ipv4->sin_addr), ip, ipSize);
    }else
    if (addr->sa_family == AF_INET6)
    {
        af = AF_INET6;
        ipv6 = (struct sockaddr_in6 *)addr;
        inet_ntop(AF_INET6, &(ipv6->sin6_addr), ip, ipSize);
    }
    if(port != NULL)//Port wird nicht benötigt, deshalb setze den Port auch nicht
    {
        *port = (af == AF_INET) ? ntohs(ipv4->sin_port) : ntohs(ipv6->sin6_port);
    }
    if(addr->sa_family != AF_INET6 && addr->sa_family != AF_INET)
    {
        printf("Unbekannte Adressfamilie.\n");
        *port = -1;  // Setze den Port auf einen ungültigen Wert, um anzuzeigen, dass die Adressfamilie unbekannt ist
        returnValue = EXIT_FAILURE;
    }
    return returnValue;
}

int getClientIpAddress(int socketFd, char *ip, size_t ipSize)
{
    struct sockaddr_storage clientIPAddr;
    socklen_t clientIPAddrLen = sizeof(clientIPAddr);
    if(getpeername(socketFd, (struct sockaddr *)&clientIPAddr, &clientIPAddrLen) == -1)
    {
        perror("getpeername in getClientIpAddress()");
        return EXIT_FAILURE;
    }
    return convertAddressToString((struct sockaddr*)&clientIPAddr, ip, ipSize, NULL);
}