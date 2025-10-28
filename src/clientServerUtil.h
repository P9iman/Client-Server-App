#ifndef _CLIENTSERVERUTIL_H
#define _CLIENTSERVERUTIL_H

#include <sys/socket.h> 

/**
 * Diese Hilfsfunktion bestimmt anhand von sockaddr* die IP Adresse (IPv4 oder IPv6) und schreibt
 * sie in den Buffer ip. Außerdem liefert die Funktion auch bei bedarf den Port. Wenn port = NULL
 * übergeben wird, dann wird port ignoriert.
 *
 * @param addr Bestimme anhand dieser Struktur die IP Adresse
 * @param ip Pointer auf Buffer in dem die IP Adresse nach dem Aufruf steht
 * @param ipSize Größe der IP Adresse, also entweder INET6_ADDRSTRLEN oder INET_ADDRSTRLEN
 * @param port Der Port über den der client oder server verbunden ist
 * @return 0 success und 1 error
 */
int convertAddressToString(struct sockaddr *addr, char *ip, size_t ipSize, int *port);

/**
 * Diese Funktion liefert dem client seine IP Adresse.
 *
 * @param socketFd Socket-FD über den die IP Adresse bestimmt wird
 * @param ip Pointer auf Buffer in dem die IP Adresse nach dem Aufruf steht
 * @param ipSize Größe der IP Adresse, also entweder INET6_ADDRSTRLEN oder INET_ADDRSTRLEN
 * @return 0 success und 1 error
 */
int getClientIpAddress(int socketFd, char *ip, size_t ipSize);

#endif