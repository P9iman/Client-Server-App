#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/utsname.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define MAX_HOSTNAME_SIZE 200
#define MSG_BUFFER_SIZE 256

void *get_in_addr(struct sockaddr *sa);
void sendGetRequest(int socketFd, char* filename);
void sendPutRequest(int socketFd, char* filename);
void sendFilesRequest(int socketFd);
void sendListRequest(int socketFd);
void sendQuitRequest(int socketFd);


int main(int argc, char** argv)
{

  char serverIP[INET6_ADDRSTRLEN];
  char *port = NULL; 
  char *serverAddr = NULL;
  char hostname[MAX_HOSTNAME_SIZE]; 
  memset(hostname, 0, MAX_HOSTNAME_SIZE); 
  struct utsname serverHostname; //alternative     
  char msgBuffer[MSG_BUFFER_SIZE];
  memset(msgBuffer, 0, MSG_BUFFER_SIZE);

    //Über diesen Socket findet die Kommunikation mit dem Server statt
  int sockfd; 

  /*socket Konfiguration mit addrinfo und getaddrinfo() */

  int getaddrinfoRetVal; 
  struct addrinfo *p;
  struct addrinfo hints; 
  struct addrinfo *clientInfo; 
  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC; 
  hints.ai_socktype = SOCK_STREAM; 
  hints.ai_protocol = IPPROTO_TCP; 
  
  int getHostNameRetVal = gethostname(hostname, sizeof(hostname));
  if(getHostNameRetVal == -1)
  {
      perror("gethostname()");
      return 1; 
  }else
  {
      printf("Client started on %s\n", hostname); 
  }

  /*Parse Server-Kontakt: also DNS-Name oder IPv4/IPv6 Adresse sowie Port des Servers*/
  if(argc != 3) //argc muss 3 sein da der Programmname immer das erste Argument ist, dann kommen DNS-Name/IPv4/6 (1. Argument) und Port (2. Argument)
  {
    printf("Connecting failed!\nUsage: ./server [DNS-Name or IPv4/IPv6 Address] [Port]\n"); 
    return 1; 
  }else
  {
    serverAddr = argv[1]; 
    port = argv[2];
  }

  getaddrinfoRetVal = getaddrinfo(serverAddr, port, &hints, &clientInfo);
  if( getaddrinfoRetVal != 0)
  {
    fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(getaddrinfoRetVal));
    return 1;
  }
    /*  getaddrinfo() liefert uns  ein Liste mit address structures.
        Wir probieren jede Adresse aus bis wir erfolgreich connect()en.
        Wenn socket(2) (or connect(2)) fehlschlägt, schließen wir den socket und
        probieren die nächste Adresse.
    */
  for(p = clientInfo; p != NULL; p = p->ai_next)
  {
    sockfd = socket(clientInfo->ai_family, clientInfo->ai_socktype, clientInfo->ai_protocol); 
    if(sockfd < 0)
    {
      continue;
    }
    if(connect(sockfd, clientInfo->ai_addr, clientInfo->ai_addrlen) == 0)
    {
      //wir konnten uns erfolgreich über einen Socket mit dem Server verbinden
      break; 
    }else
    {
        //wir konnten eine Verbindung aufbauen, somit schließe den geöffnete Socket und probiere die nächste Adresse
        close(sockfd);
    }
  }

  if (p == NULL)
  {
      freeaddrinfo(clientInfo);
      fprintf(stderr, "client: failed to connect\n");
      return 1;
  }

  inet_ntop(p->ai_family, get_in_addr((struct sockaddr*) p->ai_addr),serverIP, sizeof(serverIP));
  printf("client: connecting to %s\n", serverIP);
  //Die Liste mit Adresse wird nicht mehr benötigt
  freeaddrinfo(clientInfo);

  /**
   * In dieser Schleife werden die Befehle vom Client angenommen
   */
  while(1)
  {

  }

  return 0;
}

/**
 * @brief Diese Hilfsfunktion liefert die Socketadresse sowohl für IPv4 als auch für IPv6
*/
void *get_in_addr(struct sockaddr *sa)
{
    if(sa->sa_family == AF_INET)
    {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }else
    {
        return &(((struct sockaddr_in6*)sa)->sin6_addr);
    }
}