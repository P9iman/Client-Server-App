#include <arpa/inet.h>
#include <errno.h>
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

/**
 * @file client.c
 * @brief Dieses Programm stellt einen Client dar, der mit einem Server kommuniziert.
 * Es ermöglicht dem Benutzer, verschiedene Befehle an den Server zu senden,
 * wie z.B. das Abrufen von Dateien, das Hochladen von Dateien, das Auflisten von Dateien usw.
 * Der Client verwendet Sockets, um die Verbindung zum Server herzustellen und Daten auszutauschen.
 * 
 * @note
 * Testdaten die mit Put und Get verschickt und geholt werden können befinden sich im Ordner Data. Dieser kann in den bin
 * Ordner kopiert werden um den Pfad der Datei zuverkürzen. Für den Server macht es keinen Unterschied dieser parsed den Dateinamen. 
 * Beim Get wird der Pfad nicht benötigt, da kann man einfach Get <Dateiname.txt> eingeben
 */


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
  char *port;
  char *serverAddr;
  char msgBuffer[BUFFER_SIZE];
  memset(msgBuffer, 0, sizeof(msgBuffer));
  ssize_t recvRetVal;
  int errorCode;
  //Über diesen Socket findet die Kommunikation mit dem Server statt
  int socketFd;

  /*===== Socket Konfiguration mit addrinfo und getaddrinfo() =====*/
  int getaddrinfoRetVal;
  struct addrinfo *p;
  struct addrinfo hints;
  struct addrinfo *clientInfo;
  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  //hints.ai_protocol = IPPROTO_TCP;
  /*===== Parse Server-Kontakt: also DNS-Name oder IPv4/IPv6 Adresse sowie Port des Servers =====*/
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
  memset(input, 0, INPUT_SIZE);
  memset(filename, 0, FILENAME_SIZE);
  memset(command, 0, COMMAND_SIZE);


  printf("Geben Sie 'Get <Dateiname>' ein, um eine Datei vom Server abzurufen.\n");
  printf("Geben Sie 'Put <Dateiname>' ein, um eine Datei auf den Server hochzuladen.\n");
  printf("Geben Sie 'Files' ein, um eine Liste der verfügbaren Dateien auf dem Server abzurufen.\n");
  printf("Geben Sie 'List' ein, um alle verbundenen Clients aufzulisten.\n");
  printf("Geben Sie 'Quit' ein, um die Verbindung zu beenden.\n\n");

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
  return 0;
}

/**
 * @brief Sendet eine GET-Anforderung an den Server, um eine Datei abzurufen.
 * @param socketFd Der Socket-Dateideskriptor für die Verbindung zum Server.
 * @param filename Der Name der abzurufenden Datei.
 * @return 0 bei Erfolg, andernfalls ein Fehlercode.
 */
int sendGetRequest(int socketFd, char *filename) {
  /*===== Client sendet dem Server den Befehl und den Filenamen =====*/
  char buffer[BUFFER_SIZE];
  memset(buffer, 0, BUFFER_SIZE);
  sprintf(buffer, "Get %s", filename);
  if (send(socketFd, buffer, strlen(buffer), 0) == -1) {
    perror("send in sendGetRequest, while sending cmd and filename");
    return ERROR_GET;
  }
  /*===== Warte auf ein ACK vom Server das Befehl und Filename erfolgreich angekommen sind =====*/
  ssize_t recvRet;
  ssize_t byteSent;
  memset(buffer, 0, BUFFER_SIZE);
  recvRet = recv(socketFd, buffer, sizeof(buffer), 0);
  if(recvRet == -1){
    perror("recv in sendGetRequest, while waiting for ACK from server");
    return ERROR_GET;
  }
  if(strcmp(buffer, "NACK, File not found!") == 0){
    printf("NACK from server! File does not exist on server!\n");
    return ERROR_GET;
  }
  /*===== Sende dem Server Ready Signal damit Übertragung von Dateiattribute beginnt =====*/
  memset(buffer, 0, sizeof(buffer));
  byteSent = send(socketFd, "READY", sizeof("READY"), 0);
  if(byteSent == -1){
    perror("send in sendGetRequest, while sending READY signal to server");
    return ERROR_GET;
  }

  /*===== Bereit für Datei-Attribute =====*/
  memset(buffer, 0, sizeof(buffer));
  recvRet = recv(socketFd, buffer, sizeof(buffer), 0);
  if(recvRet == -1){
    perror("recv in sendGetRequest, while waiting for file attributes from server");
    return ERROR_GET;
  }
  /* Ausgabe der Dateiattribute */
  printf("%s\n", buffer);
  /*===== Sende Server ACK dafür das Datei-Attribute erhalten wurden =====*/
  byteSent = send(socketFd, "ACK", sizeof("ACK"), 0);
  if(byteSent == -1){
    perror("send in sendGetRequest, while sending ACK for file attributes");
    return ERROR_GET;
  }

  /*===== Bereit für Dateiinhalte =====*/
  memset(buffer, 0, sizeof(buffer));
  printf("Filecontent:\n");
  while(1)
  {
    memset(buffer, 0, sizeof(buffer));
    recvRet = recv(socketFd, buffer, sizeof(buffer), 0);
    if(recvRet == 1 && buffer[0] == '\x04' ){
      break;
    }
    if(recvRet == -1){
      byteSent = send(socketFd, "NACK", sizeof("NACK"), 0);
      if(byteSent == -1){
        perror("send in sendGetRequest, while sending NACK to server");
      }
      perror("recv in sendGetRequest, recv in while-loop");
      return ERROR_GET;
    }
    //gebe den Inhalt der Datei aus
    printf("%s", buffer);
    /*===== Schicke ACK an Server um den Erhalt des ersten Datenblocks zu bestätigen =====*/
    byteSent = send(socketFd, "ACK", sizeof("ACK"), 0);
    if(byteSent == -1){
      perror("send in sendGetRequest, while sending ack for recv filecontent");
      return ERROR_GET;
    }
  }
  printf("\n");
  return EXIT_SUCCESS;
}

/**
 * @brief Sendet eine PUT-Anforderung an den Server, um eine Datei hochzuladen.
 * @param socketFd Der Socket-Dateideskriptor für die Verbindung zum Server.
 * @param filename Der Name der hochzuladenden Datei.
 * @return 0 bei Erfolg, andernfalls ein Fehlercode.
 */
int sendPutRequest(int socketFd, char *filename) {
  /*===== Öffne zunächst die Datei =====*/
  FILE *file = fopen(filename, "rb");
  if (file == NULL) {
    perror("fopen in sendPutRequest");
    return ERROR_PUT;
  }
  /*===== Sende zunächst nur den Befehl mit dem Filenamen =====*/
  char buffer[BUFFER_SIZE];
  memset(buffer, 0, sizeof(buffer));
  char command[] = "Put";
  strncpy(buffer, command, sizeof(buffer));
  strncat(buffer, " ", sizeof(buffer) - strlen(buffer) -1);
  strncat(buffer, filename, sizeof(buffer) - strlen(buffer) -1);
  ssize_t byteSent = send(socketFd, buffer, strlen(buffer), 0);
  printf("Sent command and filename: %s\n", buffer);

  if(byteSent == -1){
    perror("send in sendPutRequest");
    fclose(file);
    return ERROR_PUT;
  }
  /*===== Warte auf ein ACK vom server das Befehl und Filename korrekt angekommen sind =====*/
  ssize_t recvRet;
  memset(buffer, 0, sizeof(buffer));
  recvRet = recv(socketFd, buffer, sizeof(buffer), 0);
  if(recvRet  == -1){
    perror("recv in sendPutRequest, receiving ACK for command and filename");
    fclose(file);
    return ERROR_PUT;
  }
  if(strcmp(buffer, "NACK") == 0){
    printf("NACK from server! Command or filename was not correct!\n");
    return ERROR_PUT;
  }
  /*===== Wenn hier angekommen, konnte Befehl und filname korrekt übertragen werden =====*/
  /*===== Beginne mit der Übertragung des Dateiinhalts =====*/
  memset(buffer, 0, sizeof(buffer));
  size_t bytesRead = 1;
  while(1){
    bytesRead = fread(buffer, 1, BUFFER_SIZE-2, file);

    if(ferror(file) != 0){
      printf("Error reading file: %s\n", filename);
      clearerr(file);
      fclose(file);
      return ERROR_PUT;
    }
    byteSent = send(socketFd, buffer, bytesRead, 0);
    if(byteSent == -1){
      perror("send in sendPutRequest, while sending filecontent");
      fclose(file);
      return ERROR_PUT;
    }
    printf("[Tranmitted filecontent]\n");
    /*===== warte nach jeder Übertragung ein Datenblocks auf ein ACK um danach weiter zu machen=====*/
    memset(buffer, 0, sizeof(buffer));
    recvRet = recv(socketFd, buffer, sizeof(buffer), 0);
    if(recvRet == -1){
      perror("recv in sendPutRequest, while recv ACK for filecontent");
      fclose(file);
      return ERROR_PUT;
    }
    if(strcmp(buffer, "NACK") == 0){
      printf("No ACK from server received! Data was not written in file\n");
      return ERROR_PUT;
    }
    if(feof(file) != 0){
      //EOF erreicht beende Datenübertragung
      clearerr(file);
      break;
    }
  }
  fclose(file);
  /*===== Die Datenübertragung des Dateiinhalts ist zuende schicke ein EOT Zeichen =====*/
  char eot = '\x04';
  byteSent = send(socketFd, &eot, sizeof(eot), 0);
  if(byteSent == -1){
    perror("send in sendPutRequest, while sending eot");
    fclose(file);
    return ERROR_PUT;
  }
  /*===== Warte ein letzes Mal auf ACK, das der Server wirklich EOT erhalten hat =====*/
  memset(buffer, 0, sizeof(buffer));
  recvRet = recv(socketFd, buffer, sizeof(buffer), 0);
  if(recvRet == -1){
    perror("recv in sendPutRequest, while recv ACK for EOT");
    return ERROR_PUT;
  }
  /*===== Warte nun auf die Bestätigung der Speicherung der Datei  =====*/
  memset(buffer, 0, sizeof(buffer));
  recvRet = recv(socketFd, buffer, sizeof(buffer), 0);
  if(recvRet == -1){
    perror("recv in sendPutRequest, while recv response from client");
    return ERROR_PUT;
  }
  printf("%s", buffer);
  return EXIT_SUCCESS;
}

/**
 * @brief Sendet eine FILES-Anforderung an den Server, um eine Liste der verfügbaren Dateien abzurufen.
 * @param socketFd Der Socket-Dateideskriptor für die Verbindung zum Server.
 * @return 0 bei Erfolg, andernfalls ein Fehlercode.
 */
int sendFilesRequest(int socketFd) {
  char buffer[] = "Files";
  if (send(socketFd, buffer, strlen(buffer), 0) == -1) {
    perror("send in sendFilesRequest");
    return ERROR_FILES;
  }
  char filesBuffer[BUFFER_SIZE];
  ssize_t recvRet;
  memset(filesBuffer, 0, sizeof(filesBuffer));
  recvRet = recv(socketFd, filesBuffer, sizeof(filesBuffer), 0);
  if(recvRet == -1){
    perror("recv in sendListRequst");
    return ERROR_FILES;
  }
  printf("%s", filesBuffer);
  return EXIT_SUCCESS;
}

/**
 * @brief Sendet eine LIST-Anforderung an den Server, um eine Liste aktuell verbundender Clients abzurufen.
 * @param socketFd Der Socket-Dateideskriptor für die Verbindung zum Server.
 * @return 0 bei Erfolg, andernfalls ein Fehlercode.
 */
int sendListRequest(int socketFd) {
  char buffer[] = "List";
  if (send(socketFd, buffer, strlen(buffer), 0) == -1) {
    perror("send in sendListRequest");
    return ERROR_LIST;
  }
  char listBuffer[BUFFER_SIZE];
  ssize_t recvRet;
  memset(listBuffer, 0, sizeof(listBuffer));
  recvRet = recv(socketFd, listBuffer, sizeof(listBuffer), 0);
  if(recvRet == -1){
    perror("recv in sendListRequst");
    return ERROR_LIST;
  }
  printf("%s", listBuffer);
  return EXIT_SUCCESS;
}

/**
 * @brief Sendet eine QUIT-Anforderung an den Server, um die Verbindung zu beenden.
 * @param socketFd Der Socket-Dateideskriptor für die Verbindung zum Server.
 * @return 0 bei Erfolg, andernfalls ein Fehlercode.
 */
int sendQuitRequest(int socketFd) {
  char *buffer = {0};
  if (send(socketFd, buffer, 0, 0) == -1) {
    perror("send in sendQuitRequest");
    return ERROR_QUIT;
  } else {
    close(socketFd);
    return EXIT_SUCCESS;
  }
}

/**
 * @brief Konvertiert eine Socket-Adresse in eine Zeichenkette.
 * @param addr Die Socket-Adresse.
 * @param ip Zeiger auf den Puffer, in den die Zeichenkette/IP-Adresse geschrieben wird.
 * @param ipSize Größe des Puffers.
 * @param port Zeiger auf den Speicherort, an dem der Port gespeichert wird.
 * @return 0 bei Erfolg, -1 bei einem Fehler.
 */
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

/**
 * @brief Ruft die IP-Adresse des Clients ab.
 * @param socketFd Der Socket-Dateideskriptor für die Verbindung zum Server.
 * @param ip Zeiger auf den Speicherort, an dem die IP-Adresse gespeichert wird.
 * @return 0 bei Erfolg, -1 bei einem Fehler.
 *  
*/
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