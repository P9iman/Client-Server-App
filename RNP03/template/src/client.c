#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netdb.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// TODO: Remove this block.
#define SRV_ADDRESS "127.0.0.1"
#define SRV_PORT 7777



int main(int argc, char** argv)
{ 
  /*Parse Server-Kontakt: also DNS-Name oder IPv4/IPv6 Adresse sowie Port des Servers*/
  
  //Der Port von dem Server über dem Nachrichten ausgetauscht werden 
  int port = 0; 
  //Die IPv4/IPv6 Adresse oder DNS-Name nur für Ausgabe
  char *serverAddr = NULL; 

  if(argc != 3) //argc muss 3 sein da der Programmname immer das erste Argument ist, dann kommen DNS-Name/IPv4/6 (1. Argument) und Port (2. Argument)
  {
      printf("Connecting failed!\nUsage: ./server [DNS-Name or IPv4/IPv6 Address] [Port]\n"); 
      return 1; 
  }else
  {
      serverAddr = argv[1]; 
      port = atoi(argv[2]); //konvertiert String zu int
      printf("Connecting to %s on Port %d ...\n", serverAddr, port);  
  }

  int retVal; 
  struct addrinfo hints; 
  struct addrinfo *serverInfo;  
  memset(&hints, 0, sizeof hints); 
  hints.ai_family = AF_UNSPEC; //Adress-Family ist unspezifiziert somit IPv
  

  int s_tcp; 
  struct sockaddr_in sa;
  unsigned int sa_len = sizeof(struct sockaddr_in);
  ssize_t n = 0;
  char* msg = "Hello World!";

  sa.sin_family = AF_INET;
  sa.sin_port = htons(SRV_PORT);

  if (inet_pton(sa.sin_family, SRV_ADDRESS, &sa.sin_addr.s_addr) <= 0) {
    perror("Address Conversion");
    return 1;
  }

  if ((s_tcp = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
    perror("TCP Socket");
    return 1;
  }

  if (connect(s_tcp, (struct sockaddr*)&sa, sa_len) < 0) {
    perror("Connect");
    return 1;
  }

  if ((n = send(s_tcp, msg, strlen(msg), 0)) > 0) {
    printf("Message %s sent (%zi Bytes).\n", msg, n);
  }

  close(s_tcp);
}
