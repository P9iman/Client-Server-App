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

#define MAX_HOSTNAME_SIZE 50
#define MSG_BUFFER_SIZE 256
#define INPUT_SIZE 200
#define COMMAND_SIZE 7
#define FILENAME_SIZE 193

#define ERROR_GET 1
#define ERROR_PUT 2
#define ERROR_FILES 3
#define ERROR_LIST 4
#define ERROR_QUIT 5

void *get_in_addr(struct sockaddr *sa);
int sendGetRequest(int socketFd, char* filename);
int sendPutRequest(int socketFd, char* filename);
int sendFilesRequest(int socketFd);
int sendListRequest(int socketFd);
int sendQuitRequest(int socketFd);


int main(int argc, char** argv)
{
    char serverIP[INET6_ADDRSTRLEN];
    char *port = NULL; 
    char *serverAddr = NULL;
    char hostname[MAX_HOSTNAME_SIZE]; 
    memset(hostname, 0, MAX_HOSTNAME_SIZE); 
    char recvMsgBuffer[MSG_BUFFER_SIZE];
    ssize_t recvRetVal = 0;
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
        socketFd = socket(clientInfo->ai_family, clientInfo->ai_socktype, clientInfo->ai_protocol);
        if(socketFd < 0)
        {
            continue;
        }
        if(connect(socketFd, clientInfo->ai_addr, clientInfo->ai_addrlen) == 0)
        {
            /*Probiere hier den Verbindungsaufbau*/
            recvRetVal = recv(socketFd, recvMsgBuffer, MSG_BUFFER_SIZE,0);   
            if(recvRetVal == 0)
            {
                printf("Server ist voll. Bitte versuche es später erneut!\n"); 
                close(socketFd);
                exit(EXIT_SUCCESS);
            }else
            if(recvRetVal > 0)
            {
                printf("\nErfolgreich verbunden\n"); 
            }else
            {
                perror("Error in recv"); 
                exit(EXIT_FAILURE); 
            }
            //wir konnten uns erfolgreich über einen Socket mit dem Server verbinden
            //printf("Es konnte sich mit dem Server verbunden werden\n ");
            break;
        }else
        {
            //wir konnten keine Verbindung aufbauen, somit schließe den geöffnete Socket und probiere die nächste Adresse
            close(socketFd);
        }
    }

    if (p == NULL)
    {
        freeaddrinfo(clientInfo);
        fprintf(stderr, "client: failed to connect\n");
        return 1;
    }
    // char hostIPAddr[MAX_HOSTNAME_SIZE];
    // struct addrinfo* ret;
    // getaddrinfo(hostname, "echo", NULL, &ret);
    // inet_ntop(AF_INET, (struct sockaddr*)ret, hostIPAddr, sizeof (hostIPAddr));
    // printf("Client IP Address: %s\n", hostIPAddr);

    struct sockaddr_in clientIPAddr; 
    socklen_t clientIPAddrLen; 
    if (getpeername(socketFd, (struct sockaddr *)&clientIPAddr, &clientIPAddrLen) == -1)
    {
        perror("Fehler beim Abrufen der Remote-Adresse");
        return 1;
    }
    char clientIP[INET_ADDRSTRLEN];
    if (inet_ntop(AF_INET, &(clientIPAddr.sin_addr), clientIP, INET_ADDRSTRLEN) == NULL) {
        perror("Fehler beim Konvertieren der IP-Adresse");
        return 1;
    }
    printf("Client IP-address: %s\n", clientIP);


    inet_ntop(p->ai_family, get_in_addr((struct sockaddr*) p->ai_addr),serverIP, sizeof(serverIP));
    printf("Client connecting to %s\n", serverIP);
    //Die Liste mit Adresse wird nicht mehr benötigt
    freeaddrinfo(clientInfo);

    char input[INPUT_SIZE];
    char filename[FILENAME_SIZE];
    char command[COMMAND_SIZE];
    char* fgetsRetVal;
  /**
   * In dieser Schleife werden die Befehle vom Client angenommen
   */
  while(1)
  {
    printf("Client ist Ready...\n");   
    fgetsRetVal =  fgets(input, INPUT_SIZE, stdin);
      if(fgetsRetVal == NULL)
      {
          fprintf(stderr, "Error: Reading Input from stdin in Line: %d\n", __LINE__);
      }
      //input[strcspn(input, "\n")] = '\0';

      // Eingabe in Befehl und Dateinamen aufteilen
      sscanf(input, "%s %s", command, filename);

      // Befehl überprüfen und entsprechende Aktion ausführen
      if (strcmp(command, "List") == 0)
      {
          errorCode =  sendListRequest(socketFd);
      }else
      if (strcmp(command, "Get") == 0)
      {
          if (strlen(filename) == 0)
          {
              printf("Bitte geben Sie einen Dateinamen ein!\n");
              continue;
          }
          errorCode = sendGetRequest(socketFd, filename);
      }else
      if (strcmp(command, "Put") == 0)
      {
          if (strlen(filename) == 0)
          {
              printf("Bitte geben Sie einen Dateinamen ein!\n");
              continue;
          }
          errorCode = sendPutRequest(socketFd, filename);
      } else if (strcmp(command, "Files") == 0)
      {
        errorCode = sendFilesRequest(socketFd);
      }else
      if (strcmp(command, "Quit") == 0)
      {
          if(sendQuitRequest(socketFd) == ERROR_QUIT)
          {
              printf("Quit-Request konnte nicht geschickt werden!\n");
              continue;
          }
          break;
      }else
      {
          printf("Ungültiger Befehl!\n");
          continue;
      }if(errorCode == 0)
      {
          recvRetVal =  recv(socketFd,recvMsgBuffer,MSG_BUFFER_SIZE, 0);
          if(recvRetVal < 0)
          {
              perror("recv Msg from Server");
          }else{
              //Gib die Msg aus
              printf("%s", recvMsgBuffer);
          }
      }else
      {
          switch(errorCode)
          {
              case ERROR_LIST : printf("List-Request konnte nicht geschickt werden!\n");
              break;
              case ERROR_FILES : printf("Files-Request konnte nicht geschickt werden!\n");
                break;
              case ERROR_PUT : printf("Put-Request konnte nicht geschickt werden!\n");
                break;
              case ERROR_GET : printf("Get-Request konnte nicht geschickt werden!\n");
                  break;
              default: printf("Unknown Error\n");
                break;
          }
          continue;
      }
  }
  return 0;
}

int sendGetRequest(int socketFd, char* filename)
{
    char msgBuffer[MSG_BUFFER_SIZE];
    memset(msgBuffer, 0, MSG_BUFFER_SIZE);
    //1. send des Request
    sprintf(msgBuffer, "Get %s", filename);
    if(send(socketFd, msgBuffer, strlen(msgBuffer), 0) == -1)
    {
        perror("send");
        return ERROR_GET;
    }
    return EXIT_SUCCESS;
}

int sendPutRequest(int socketFd, char* filename)
{
    char msgBuffer[MSG_BUFFER_SIZE+1];
    memset(msgBuffer, 0, MSG_BUFFER_SIZE);
    FILE* file =  fopen(filename, "r");
    if(file == NULL)
    {
        perror("Fehler beim öffnen. Datei existiert wohlmöglich nicht");
        return ERROR_PUT;
    }else
    {
        size_t newLen = fread(msgBuffer, sizeof(char), MSG_BUFFER_SIZE, file);
        if (ferror( file ) != 0 )
        {
            fputs("Error reading file", stderr);
            return ERROR_PUT;
        }else
        {
            msgBuffer[newLen++] = '\0'; /* Just to be safe. */
        }
        if(send(socketFd, msgBuffer, strlen(msgBuffer), 0) == -1)
        {
            perror("send");
            return ERROR_PUT;
        }
        return EXIT_SUCCESS;
    }
}

int sendFilesRequest(int socketFd)
{
    char msgBuffer[] = "Files";
    if(send(socketFd, msgBuffer, strlen(msgBuffer), 0) == -1)
    {
        perror("send");
        return ERROR_FILES;
    }
    return EXIT_SUCCESS;
}

int sendListRequest(int socketFd)
{
    char msgBuffer[] = "List";
    if(send(socketFd, msgBuffer, strlen(msgBuffer), 0) == -1)
    {
        perror("send");
        return ERROR_LIST;
    }
    return EXIT_SUCCESS;
}

int sendQuitRequest(int socketFd)
{
    char* msgBuffer = {0};
    if(send(socketFd,msgBuffer, 0, 0) == -1)
    {
        return ERROR_QUIT;
    }else{
        close(socketFd);
        return EXIT_SUCCESS;
    }
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