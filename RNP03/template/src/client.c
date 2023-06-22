#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

//#include "clientServerUtil.h"

#define BUFFER_SIZE 256
#define INPUT_SIZE 200
#define COMMAND_SIZE 7
#define FILENAME_SIZE 100

#define ERROR_GET 1
#define ERROR_PUT 2
#define ERROR_FILES 3
#define ERROR_LIST 4
#define ERROR_QUIT 5

int sendGetRequest(int socketFd, char *filename);
int sendPutRequest(int socketFd, char *filename);
int sendFilesRequest(int socketFd);
int sendListRequest(int socketFd);
int sendQuitRequest(int socketFd);

int convertAddressToString(struct sockaddr *, char *, size_t, int *);
int getClientIpAddress(int, char *, size_t);

int main(int argc, char **argv) {
  char serverIP[INET6_ADDRSTRLEN];
  char clientIP[INET6_ADDRSTRLEN];
  memset(serverIP, 0, sizeof(serverIP));
  memset(clientIP, 0, sizeof(clientIP));
  char *port = NULL;
  char *serverAddr = NULL;
  char msgBuffer[BUFFER_SIZE];
  ssize_t recvRetVal;
  int errorCode;

  //Über diesen Socket findet die Kommunikation mit dem Server statt
  int socketFd;

  /*socket Konfiguration mit addrinfo und getaddrinfo() */

  int getaddrinfoRetVal;
  struct addrinfo *p;
  struct addrinfo hints;
  struct addrinfo *clientInfo;
  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol = IPPROTO_TCP;

  /*Parse Server-Kontakt: also DNS-Name oder IPv4/IPv6 Adresse sowie Port des
   * Servers*/
  if (argc != 3)
  {
    printf("Connecting failed!\nUsage: ./client [DNS-Name or IPv4/IPv6 "
           "Address] [Port]\n");
    return 1;
  } else {
    serverAddr = argv[1];
    port = argv[2];
  }
  getaddrinfoRetVal = getaddrinfo(serverAddr, port, &hints, &clientInfo);
  if (getaddrinfoRetVal != 0) {
    fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(getaddrinfoRetVal));
    return 1;
  }
  /*  getaddrinfo() liefert uns  ein Liste mit address structures.
      Wir probieren jede Adresse aus bis wir erfolgreich connect()en.
      Wenn socket(2) (or connect(2)) fehlschlägt, schließen wir den socket und
      probieren die nächste Adresse.
  */
  for (p = clientInfo; p != NULL; p = p->ai_next) {
    socketFd = socket(clientInfo->ai_family, clientInfo->ai_socktype, 0);
    if (socketFd < 0) {
      continue;
    }
    if (connect(socketFd, clientInfo->ai_addr, clientInfo->ai_addrlen) == 0) {
      /*Probiere hier den Verbindungsaufbau*/
      recvRetVal = recv(socketFd, msgBuffer, BUFFER_SIZE, 0);
      if (recvRetVal == 0) {
        printf("Server ist voll. Bitte versuche es später erneut!\n");
        close(socketFd);
        exit(EXIT_SUCCESS);
      }
      if (recvRetVal < 0) {
        perror("Error in recv");
        exit(EXIT_FAILURE);
      }
      // wir konnten uns erfolgreich über einen Socket mit dem Server verbinden
      break;
    } else {
      // wir konnten keine Verbindung aufbauen, somit schließe den geöffnete
      // Socket und probiere die nächste Adresse
      close(socketFd);
    }
  }

  if (p == NULL) {
    freeaddrinfo(clientInfo);
    fprintf(stderr, "client: failed to connect\n");
    return 1;
  }
  if (getClientIpAddress(socketFd, clientIP, sizeof(clientIP)) != 0) {
    printf("Error in getClientIpAddress!\n");
    freeaddrinfo(clientInfo);
    return EXIT_FAILURE;
  }
  printf("Client IP-address: %s\n", clientIP);
  convertAddressToString(p->ai_addr, serverIP, sizeof(serverIP), NULL);
  printf("Client connecting to %s\n", serverIP);
  // Die Liste mit Adresse wird nicht mehr benötigt
  freeaddrinfo(clientInfo);

  char input[INPUT_SIZE];
  char filename[FILENAME_SIZE];
  char command[COMMAND_SIZE];
  char *fgetsRetVal;
  /**
   * In dieser while(1) Schleife werden die Befehle des Clienten
   * entgegengenommen
   */
  while (1) {
    memset(msgBuffer, 0, sizeof(msgBuffer));
    fgetsRetVal = fgets(input, INPUT_SIZE, stdin);
      if (fgetsRetVal == NULL) {
          fprintf(stderr, "Error: Reading Input from stdin in Line: %d\n", __LINE__);
      }

      // Eingabe in Befehl und Dateinamen aufteilen
    sscanf(input, "%s %s", command, filename);

    // Befehl überprüfen und entsprechende Aktion ausführen
    if (strcmp(command, "List") == 0) {
      errorCode = sendListRequest(socketFd);
    } else if (strcmp(command, "Get") == 0) {
      if (strlen(filename) == 0) {
        printf("Bitte geben Sie einen Dateinamen ein!\n");
        continue;
      }
      errorCode = sendGetRequest(socketFd, filename);
    } else if (strcmp(command, "Put") == 0) {
      if (strlen(filename) == 0) {
        printf("Bitte geben Sie einen Dateinamen ein!\n");
        continue;
      }
      errorCode = sendPutRequest(socketFd, filename);
    } else if (strcmp(command, "Files") == 0) {
      errorCode = sendFilesRequest(socketFd);
    } else if (strcmp(command, "Quit") == 0) {
      if (sendQuitRequest(socketFd) == ERROR_QUIT) {
        printf("Quit-Request konnte nicht geschickt werden!\n");
        continue;
      }
      break;
    } else {
      printf("Ungültiger Befehl!\n");
      continue;
    }
    if (errorCode == 0) {
      // printf("Warte auf Response vom Server\n");
      recvRetVal = recv(socketFd, msgBuffer, BUFFER_SIZE, 0);
      if (recvRetVal < 0) {
        perror("recv Msg from Server");
        return EXIT_FAILURE;
      } else {
        // Gib die Msg aus
        printf("%s", msgBuffer);
      }
    } else {
      switch (errorCode) // NOLINT(hicpp-multiway-paths-covered)
      {
      case ERROR_LIST:
        printf("List-Request konnte nicht geschickt werden!\n");
        break;
      case ERROR_FILES:
        printf("Files-Request konnte nicht geschickt werden!\n");
        break;
      case ERROR_PUT:
        printf("Put-Request konnte nicht geschickt werden!\n");
        break;
      case ERROR_GET:
        printf("Get-Request konnte nicht geschickt werden!\n");
        break;
      }
      continue;
    }
  }
  return 0;
}

int sendGetRequest(int socketFd, char *filename) {
  char msgBuffer[BUFFER_SIZE];
  memset(msgBuffer, 0, BUFFER_SIZE);
  // 1. send des Request
  sprintf(msgBuffer, "Get %s", filename);
  if (send(socketFd, msgBuffer, strlen(msgBuffer), 0) == -1) {
    perror("send");
    return ERROR_GET;
  }
  return EXIT_SUCCESS;
}

int sendPutRequest(int socketFd, char *filename) {
  /*===== Öffne zunächst die Datei =====*/
  FILE *file = fopen(filename, "rb");
  if (file == NULL) {
    perror("fopen in sendPutRequest");
    return ERROR_PUT;
  }
  /*===== Sende zunächst nur den Befehl mit dem Filenamen =====*/
  char msgBuffer[BUFFER_SIZE];
  memset(msgBuffer, 0, sizeof(msgBuffer));
  char command[] = "Put";
  strncpy(msgBuffer, command, sizeof(msgBuffer));
  strncat(msgBuffer, " ", sizeof(msgBuffer) - strlen(msgBuffer) -1);
  strncat(msgBuffer, filename, sizeof(msgBuffer) - strlen(msgBuffer) -1);
  ssize_t byteSent = send(socketFd, msgBuffer, strlen(msgBuffer), 0);
  if(byteSent == -1){
    perror("send in sendPutRequest");
    fclose(file);
    return ERROR_PUT;
  }

  /*===== Warte nun auf das ACK vom Server, dass Filename und Command korrekt übertragen wurden =====*/
  ssize_t recvRet;
  /*
  memset(msgBuffer, 0, sizeof(msgBuffer));
  recvRet = recv(socketFd, msgBuffer, sizeof(msgBuffer), 0);
  if(recvRet == -1){
    perror("recv in sendPutRequest");
    return ERROR_PUT;
  }
  if(strcmp(msgBuffer, "NACK") == 0){
    printf("No ACK from server received!\n");
    return ERROR_PUT;
  }
*/

  /*===== ACK vom Server erhalten beginne mit der Übertragung des Dateiinhalts =====*/
  memset(msgBuffer, 0, sizeof(msgBuffer));
  size_t bytesRead;
  while((bytesRead = fread(msgBuffer, sizeof(char), BUFFER_SIZE, file)) > 0){
    byteSent = send(socketFd, msgBuffer, bytesRead, 0);
    if(byteSent == -1){
      perror("send in sendPutRequest");
      fclose(file);
      return ERROR_PUT;
    }
    printf("Tranmitted filecontent\n");
    /*===== warte nach jeder Übertragung ein Datenblocks auf ein ACK um danach weiter zu machen=====*/
    memset(msgBuffer, 0, sizeof(msgBuffer));
    recvRet = recv(socketFd, msgBuffer, sizeof(msgBuffer), 0);
    if(recvRet == -1){
      perror("recv in sendPutRequest");
      return ERROR_PUT;
    }
    if(strcmp(msgBuffer, "NACK") == 0){
      printf("No ACK from server received!\n");
      return ERROR_PUT;
    }
  }
  fclose(file);
  /*===== Die Datenübertragung des Dateiinhalts ist zuende schicke ein EOT Zeichen =====*/
  char eot = '\x04';
  byteSent = send(socketFd, &eot, sizeof(eot), 0);
  if(byteSent == -1){
    perror("send in sendPutRequest");
    fclose(file);
    return ERROR_PUT;
  }
  /*===== Warte ein letzes Mal auf ACK, das der Server wirklich EOT erhalten hat =====*/
  memset(msgBuffer, 0, sizeof(msgBuffer));
  recvRet = recv(socketFd,msgBuffer, sizeof(msgBuffer), 0);
  if(recvRet == -1){
    perror("recv in sendPutRequest");
    return ERROR_PUT;
  }
  if(strcmp(msgBuffer, "NACK") == 0){
    printf("No ACK from server received! EOT not received!\n");
    return ERROR_PUT;
  }
  return EXIT_SUCCESS;
}

int sendFilesRequest(int socketFd) {
  char msgBuffer[] = "Files";
  if (send(socketFd, msgBuffer, strlen(msgBuffer), 0) == -1) {
    perror("send");
    return ERROR_FILES;
  }
  return EXIT_SUCCESS;
}

int sendListRequest(int socketFd) {
  char msgBuffer[] = "List";
  if (send(socketFd, msgBuffer, strlen(msgBuffer), 0) == -1) {
    perror("send");
    return ERROR_LIST;
  }
  return EXIT_SUCCESS;
}

int sendQuitRequest(int socketFd) {
  char *msgBuffer = {0};
  if (send(socketFd, msgBuffer, 0, 0) == -1) {
    return ERROR_QUIT;
  } else {
    close(socketFd);
    return EXIT_SUCCESS;
  }
}

int convertAddressToString(struct sockaddr *addr, char *ip, size_t ipSize,
                           int *port) {
  int returnValue = EXIT_SUCCESS;
  int af = 0;
  struct sockaddr_in *ipv4 = NULL;
  struct sockaddr_in6 *ipv6 = NULL;
  if (addr->sa_family == AF_INET) {
    af = AF_INET;
    ipv4 = (struct sockaddr_in *)addr;
    inet_ntop(AF_INET, &(ipv4->sin_addr), ip, ipSize);
  } else if (addr->sa_family == AF_INET6) {
    af = AF_INET6;
    ipv6 = (struct sockaddr_in6 *)addr;
    inet_ntop(AF_INET6, &(ipv6->sin6_addr), ip, ipSize);
  }
  if (port !=
      NULL) // Port wird nicht benötigt, deshalb setze den Port auch nicht
  {
    *port = (af == AF_INET) ? ntohs(ipv4->sin_port) : ntohs(ipv6->sin6_port);
  }
  if (addr->sa_family != AF_INET6 && addr->sa_family != AF_INET) {
    printf("Unbekannte Adressfamilie.\n");
    *port = -1; // Setzen Sie den Port auf einen ungültigen Wert, um anzuzeigen,
                // dass die Adressfamilie unbekannt ist
    returnValue = EXIT_FAILURE;
  }
  return returnValue;
}

int getClientIpAddress(int socketFd, char *ip, size_t ipSize) {
  struct sockaddr_storage clientIPAddr;
  socklen_t clientIPAddrLen = sizeof(clientIPAddr);
  if (getpeername(socketFd, (struct sockaddr *)&clientIPAddr,
                  &clientIPAddrLen) == -1) {
    perror("getpeername in getClientIpAddress()");
    return EXIT_FAILURE;
  }
  return convertAddressToString((struct sockaddr *)&clientIPAddr, ip, ipSize,
                                NULL);
}