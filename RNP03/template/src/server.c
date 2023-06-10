#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/select.h>
#include <sys/utsname.h>
#include <netdb.h>
#include <dirent.h>
#include <sys/stat.h>
#include <fcntl.h>

#define DEFAULT_PORT 0
#define BUFFER_SIZE 256
#define HOSTNAME_SIZE 50

#define EOT '\x04'

/*Prototypen*/

void handleList(int socketFd);
void handleFiles(int socketFd);
void handleGet(char *arg, int socketFd);
void handlePut(char *arg, int socketFd, int dataLength);
void *get_in_addr(struct sockaddr *sa);
void setnonblocking(int socket);

struct utsname serverHostname;
int sfd_listener; /*sfd_listener = Socket-File-Diskreptor für listening*/
int readSockets = 0; /*Hier drinn steht der RetVal von select, also die Anzahl an readable Sockets*/
int connectlist[5];  /* Array of connected sockets so we know who we are talking to */

fd_set read_fdset; /*in diesem Set mit File-Discreptoren sind die fds für die Sockets aus denen gelesen werden soll*/

int maxFd; /*der größte FD, wird für select benötigt*/
//sockaddr_storage besser als sockaddr_in, da es Platz für IPv4 als auch IPv6 struct hat
struct sockaddr_storage sa_client;
socklen_t sa_len = sizeof(struct sockaddr_storage);
//Die IPv4 oder IPv6 Adresse vom client
char clientIPAddress[INET6_ADDRSTRLEN];
//Die IP-Adresse vom server
char serverIPAddress[INET6_ADDRSTRLEN];
//Der Port von dem Server über dem Nachrichten ausgetauscht werden
char* serverPort;
//Buffer für die Nachricht des Clienten
char msgBuffer[BUFFER_SIZE];
//Return Value vom recv
ssize_t recvRetVal;



void setnonblocking(int socket)
{
    int opts;
    opts = fcntl(socket,F_GETFL);
    if (opts < 0) {
        perror("fcntl(F_GETFL)");
        exit(EXIT_FAILURE);
    }
    opts = (opts | O_NONBLOCK);
    if (fcntl(socket,F_SETFL,opts) < 0){
        perror("fcntl(F_SETFL)");
        exit(EXIT_FAILURE);
    }
}

void build_select_list()
{
    int listnum;
    FD_ZERO(&read_fdset);
    //fuege den Socket fd in das read_fdset
    FD_SET(sfd_listener, &read_fdset);
    for (listnum = 0; listnum < 5; listnum++)
    {
        if (connectlist[listnum] != 0)
        {
            FD_SET(connectlist[listnum],&read_fdset);
            if (connectlist[listnum] > maxFd)
                maxFd = connectlist[listnum];
        }
    }
}

void handle_new_connection()
{
    int listnum;
    int newConnection;
    newConnection = accept(sfd_listener, (struct sockaddr *)&sa_client, &sa_len);
    if(newConnection < 0)
    {
        perror("Error in accept");
        exit(EXIT_FAILURE);
    }
    setnonblocking(newConnection);
    for(listnum = 0; (listnum < 5) && (newConnection != -1); listnum++)
    {
        if(connectlist[listnum] == 0)
        {
            char const *inet_ntop_retVal;
            //printen die IP-Adresse mittels inet_ntop von dem Clienten um sicher zugehen das, das accept erfolgreich war.
            inet_ntop_retVal = inet_ntop(sa_client.ss_family, get_in_addr((struct sockaddr *) &sa_client),
                                         clientIPAddress, INET6_ADDRSTRLEN);
            inet_ntop_retVal == NULL ? perror("inet_ntop Fehler beim Konvertieren der Adresse") :
            printf("\nClient hat sich verbunden mit der Adresse: %s am Socket: %d\n", clientIPAddress, newConnection);
            connectlist[listnum] = newConnection;
            newConnection = -1;
        }
        if(newConnection != -1)
        {
            /* No room left in the queue! */
            printf("\nNo room left for new client.\n");
            //sock_puts(newConnection,"Sorry, this server is too busy.  Try again later!\r\n");
            close(newConnection);
        }
    }
}

void deal_with_data(int listnum)
{
    if ((recvRetVal = recv(connectlist[listnum], msgBuffer, sizeof(msgBuffer), 0)) > 0)
    {
        //TODO Auf EOT Zeichen achten

        if(strncmp("Get ", msgBuffer, 4) == 0)
        {
            handleGet(msgBuffer, connectlist[listnum]);
        }else
        if(strncmp("Put ", msgBuffer, 4) == 0)
        {
            handlePut(msgBuffer, connectlist[listnum], sizeof(msgBuffer));
        }else
        if(strncmp("Files", msgBuffer, 5) == 0)
        {
            handleFiles(connectlist[listnum]);
        }else
        if(strncmp("List", msgBuffer, 4) == 0)
        {
            handleList(connectlist[listnum]);
        }
    }else
    if(recvRetVal == 0)
    {
        /*Wir haben als Return Value von recv() 0 erhalten, das bedeutet ein Client hat die Verbindung zum Server geschlossen*/
        printf("Client am Socket %d hat Verbindung abgebrochen\n", connectlist[listnum]);
        printf("Schließe Socket %d\n", connectlist[listnum]);
        close(connectlist[listnum]);
        FD_CLR(connectlist[listnum], &read_fdset); //Lösche den fd aus dem Set
        connectlist[listnum] = 0;
    }else
    {
        //Fehler beim Empfangen der Nachricht
        perror("recv");
        close(connectlist[listnum]);
        exit(EXIT_FAILURE);
    }
}


void read_sockets()
{
    int listnum;
    if(FD_ISSET(sfd_listener, &read_fdset))
    {
        handle_new_connection();
    }
    /* for (all entries in queue) */
    for(listnum = 0; listnum < 5; listnum++){
        if(FD_ISSET(connectlist[listnum], &read_fdset))
            deal_with_data(listnum);
    }
}



int main(int argc, char** argv)
{
	memset(msgBuffer, 0, BUFFER_SIZE);
    int yes = 1; // for setsockopt() SO_REUSEADDR, below

	//Speicher den Serverhostname in serverHostname ab. 
	if(uname(&serverHostname) == -1) 
	{
        perror("Error in uname");
        return 1;    
    }
	if(argc > 2)
	{
		printf("Starting server failed!\nUsage: ./server [Port]\n"); 
      	return 1; 
	}else 
	if(argc == 2)
	{
		serverPort = argv[1]; 
	}else
	{
		serverPort = DEFAULT_PORT; 
	}
	
	/*Konfiguriere Socket*/
    int getddrinfoRetVal;
	struct addrinfo *p;			  //Index vom aktuelle Node der verketten Liste
	struct addrinfo hints; 		  //in diesem Struct steht wie der Socket konfiguriert werden soll
  	struct addrinfo *serverInfo;  //Pointer auf eine Verkettet Liste nach dem getaddrinfo() Aufruf
  	memset(&hints, 0, sizeof hints); 
  	hints.ai_family = AF_UNSPEC; //Adress-Family ist unspezifiziert somit IPv4 und IPv6 möglich
	hints.ai_socktype = SOCK_STREAM; 
	hints.ai_flags = AI_PASSIVE; //befüllt die IP Adresse fuer mich
	hints.ai_protocol = IPPROTO_TCP; 

	/**
	 * Als hostname wird hier NULL übergeben, abhängig von den ai_flags
	 * wird die IP-Adresse 0.0.0.0 oder 127.0.0.1 verwendet.
	*/
	if((getddrinfoRetVal = getaddrinfo(NULL,serverPort, &hints, &serverInfo)) != 0)
	{
		fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(getddrinfoRetVal));
		return 1; 
	}
    //Printe die IP Adresse vom Server
    inet_ntop(serverInfo->ai_family, get_in_addr(serverInfo->ai_addr), serverIPAddress, INET6_ADDRSTRLEN);
    printf("Server IP Adresse: %s\n", serverIPAddress);

    for(p = serverInfo; p != NULL; p = p->ai_next)
	{
        //TODO Hier Änderung vorgenommen letzte Argument von Socket
		sfd_listener = socket(p->ai_family, p->ai_socktype, 0);
        printf("FD vom listenere Socket: %d\n", sfd_listener);
		if(sfd_listener < 0)
		{
			continue;
		}
        //damit die Erro-Meldung: "address already in use" nicht vorkommet
        setsockopt(sfd_listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
        setnonblocking(sfd_listener);
		if(bind(sfd_listener, (struct sockaddr*)p->ai_addr, p->ai_addrlen) == 0)
		{
            //Der fd sfd_listener konnte erfolgreich mit der Socketadresse gebunden werden
            printf("Binding SocketFD: %d to sockAddr succeeded!\n", sfd_listener);
			break;
		}else
        {
            close(sfd_listener);
        }
	}
    freeaddrinfo(serverInfo); /*Die Liste mit Adresse wird nicht mehr benötigt*/

	if(p == NULL) //Keine Adresse konnte verwendet werden
	{
		fprintf(stderr, "selectserver: failed to bind\n");
		return EXIT_FAILURE;
	}
	/**
	 * Eingehende Verbindungen werden in einer Queue eingefügt bis diese mit accept() akzeptiert werden. 
	 * Mit dem Argument 5, wird die Queuegröße für die eingehenden Verbindungen festgelegt 
	*/ 
	if(listen(sfd_listener, 5) < 0) 
	{
		perror("listen");
		if(close(sfd_listener) < 0)
		{
			perror("close"); 
		}
		return EXIT_FAILURE;
	}
    maxFd = sfd_listener; /*der letzte fd ist der größte somit sfd_listener*/
    printf("Waiting for TCP connections ... \n");
    memset((char *) &connectlist, 0, sizeof(connectlist));
    while (1)
	{
        build_select_list();
        readSockets = select(FD_SETSIZE, &read_fdset, NULL, NULL, NULL);
        if(readSockets == -1)
        {
            perror("Fehler bei select()");
            return EXIT_FAILURE;
        }
        if(readSockets == 0)
        {
            //kein Socket ist ready für read. Zeige einfach das der Server noch läuft.
            printf(".");
            fflush(stdout);
        }else
        {
            read_sockets();
        }
        /**
         *  Iteriere über die Menge der FD um entweder auf neue Verbindungen zu reagieren (also wenn sich der fd Zustand von sfd_listener verändert hat)
         *  oder wenn einer der Socket FD für communication ready ist, darauf zu reagieren um Nachrichten (mit recv) zu erhalten.
         */
	}
}

/**
 * @brief Diese Funktion handled die Get Anfragen von den Clients. 
 * Es wird der Inhalt der angefragten Datei <dateiname> sowie <Datei-Attribute: last modified, size> 
 * vom Server zurückgegeben. 
 * 
 * @return 
 *  
*/
void handleGet(char *arg, int socketFd)
{
    //Parse den Filename
	char* filename = strtok(arg + 4, " ");
	FILE *file = fopen(filename, "r");
	if(file == NULL)
	{
		perror("Fehler beim Öffnen der Datei");
        exit(EXIT_FAILURE);
	}
	int fseekRetVal = fseek(file, 0, SEEK_END);
    if(fseekRetVal != 0)
    {
        perror("Error in fseek");
        exit(EXIT_FAILURE);
    }

	long fileSize = ftell(file);
    if(fileSize == -1){
        perror("Error in ftell");
        exit(EXIT_FAILURE);
    }

    fseekRetVal = fseek(file, 0, SEEK_SET);
    if(fseekRetVal != 0)
	{
		perror("Error in fseek");
        exit(EXIT_FAILURE);
	} 

	char* fileData = (char*)malloc(fileSize); 
	if(fileData == NULL)
	{
		perror("Fehler bei der Speicherzuweisung"); 
		if(fclose(file) != 0)
		{
			perror("Error in flclose"); 
		}
        exit(EXIT_FAILURE);
	}

	size_t bytesRead = fread(fileData, sizeof(char), fileSize, file); 
	if(bytesRead != (size_t)fileSize)
	{
		perror("Fehler beim Lesen der Datei"); 
		if(fclose(file) != 0)
		{
			perror("Error in flclose"); 
		} 
		free(fileData);
        exit(EXIT_FAILURE);
	}
	if(fclose(file) != 0)
	{
		perror("Error in fclose");
        exit(EXIT_FAILURE);
	} 
	// Aktuelle Uhrzeit ermitteln
    time_t currentTime;
    currentTime = time(&currentTime);

	struct tm* timeInfo = localtime(&currentTime);
    char formattedTime[64];
    size_t strftimeretVal;
	strftimeretVal =  strftime(formattedTime, sizeof(formattedTime), "%d-%m-%Y %H:%M:%S", timeInfo);
	if(strftimeretVal == 0)
	{
		printf("strftime in line: %d returned 0\n", __LINE__); 
	}

	//Response-Nachricht erstellen 
	char *response = (char*)malloc(fileSize + 256); 
	if(response == NULL)
	{
		perror("Fehler bei der Speicherzuweisung");
        exit(EXIT_FAILURE);
	}
	sprintf(response, "Datei: %s\nDateigröße: %ld\nLetzte Änderung: %s\n\n%s", filename, fileSize, formattedTime, fileData); 
	if (send(socketFd, response, strlen(response), 0) == -1)
    {
        perror("Fehler beim Senden der Daten");
        free(fileData);
        free(response);
        exit(EXIT_FAILURE);
    }
	free(fileData); 
	free(response); 
}

/**
 * @brief Diese Funktion handled die Put Anfragen der Clienten. 
 * Es wird der Inhalt der Datei <dateiname> der vom Client verschickt wurde auf dem Server gespeichert. 
 * Nach vollständigem Empfang und Speicherung der Datei wird mit einem OK, <Benutzte Server-IP-Adresse> und <Datum + Uhrzeit> 
 * das Put bestätigt. 
 * 
 * @return
*/
void handlePut(char *arg, int socketFd, int dataLength)
{
	//Parse den Dateinamen aus der Nachricht
	char* filename = strtok(arg + 4, " ");

	FILE *file = fopen(filename, "w");
	if(file == NULL)
	{
		perror("Fehler beim Öffnen der Datei");
        exit(EXIT_FAILURE);
	}

	//Dateiinhalt parsen
	char dateiInhalt[BUFFER_SIZE-4]; //252
    memcpy(dateiInhalt, arg + 4 + strlen(filename), 200);

	size_t bytesWritten = fwrite(dateiInhalt, sizeof(char), dataLength-3, file); 
	if (bytesWritten != (size_t)dataLength)
    {
        perror("Fehler beim Schreiben der Daten in die Datei");
        fclose(file);
        exit(EXIT_FAILURE);
    }
	fclose(file); 
	printf("Datei lokal auf den Server gespeichert.\n"); 

    //Response generieren
	char response[256]; 
	struct sockaddr_in server_addr; 
	socklen_t len = sizeof(server_addr); 
	getsockname(socketFd, (struct sockaddr*)&server_addr, &len); 
	char serverIP[INET_ADDRSTRLEN]; 
	inet_ntop(AF_INET, &(server_addr.sin_addr), serverIP, INET_ADDRSTRLEN);

	// Aktuelle Zeit abrufen
	time_t t = time(NULL); 
	struct tm* timeinfo = localtime(&t);
	char timerString[80]; 
	strftime(timerString, sizeof(timerString), "%d-%m-%Y %H:%M:%S", timeinfo); 

	snprintf(response, sizeof(response), "OK %s, Server IP: %s, Date: %s %s", serverHostname.machine, serverIP, __DATE__, timerString);
	send(socketFd, response, sizeof(response), 0);
}

/**
 * @brief Diese Funktion handled die List Anfragen der Clienten. 
 * Beim list werden alle verbundenen Clients der Form: 
 * <Clienthostname>:<Clientport>
 * <N> Clients verbunden
 * ausgegeben. 
 *
*/
void handleList(int socketFd)
{
    //char msgBuffer[BUFFER_SIZE];
    int port;
    char clientHostName[HOSTNAME_SIZE];
    struct sockaddr_storage sa_client_addr;
    socklen_t sa_len_addr = sizeof(struct sockaddr_storage);
    for(int i = 0; i <= maxFd; i++)
    {
        int retVal = getpeername(i, (struct sockaddr*)&sa_client_addr, &sa_len_addr);
        if(retVal == -1){
            perror("Error in getpeername");
            continue;
        }else
        {
            getnameinfo((struct sockaddr*)&sa_client,sa_len,clientHostName, sizeof(clientHostName), NULL, 0, 0);
            if(sa_client.ss_family == AF_INET)
            {
                struct sockaddr_in* sa_IPv4_client = (struct sockaddr_in*)&sa_client;
                port = sa_IPv4_client->sin_port;
            }else
            {
                struct sockaddr_in6* sa_IPv6_client = (struct sockaddr_in6*)&sa_client;
                port = sa_IPv6_client->sin6_port;
            }
            sprintf(msgBuffer, "%s : %d\n",clientHostName, port);
        }
    }
    sprintf(msgBuffer, "%d Clients verbunden\n", maxFd);

    if(send(socketFd, msgBuffer, sizeof(msgBuffer), 0) == -1)
    {
        perror("Error in send");
        exit(EXIT_FAILURE);
    }
}

/**
 * @brief Diese Funktion handled die Files Anfragen der Clienten. 
 * Bei Files wird eine Liste aller Dateien im Server-Verzeichnis in der Form: 
 * <Dateiname> <Datei-Attribute: last modified, size>
 * <N> Dateien
 * ausgegeben. 
 * 
 * @return
*/
void handleFiles(int socketFd)
{
    char directory[] = ".";
    //char msgBuffer[BUFFER_SIZE];
    DIR *dir;
    struct dirent *entry;
    struct stat file_stat;
    struct tm *modified_time;
    char modified_time_str[20];
    int numFiles = 0;
    // Verzeichnis öffnen
    dir = opendir(directory);
    if (dir == NULL) {
        perror("Fehler beim Öffnen des Verzeichnisses");
        exit(EXIT_FAILURE);
    }

    // Alle Einträge im Verzeichnis durchlaufen
    while ((entry = readdir(dir)) != NULL)
    {
        // Kompletten Dateipfad erstellen
        char file_path[256];
        //sprintf(file_path, "%s/%s", directory, entry->d_name);
        snprintf(file_path, sizeof(file_path), "%s/%s", directory, entry->d_name);

        // Dateiattribute abrufen
        if (stat(file_path, &file_stat) == -1)
        {
            perror("Fehler beim Abrufen von Dateiattributen");
            exit(EXIT_FAILURE);
        }

        // Nur reguläre Dateien berücksichtigen
        if (S_ISREG(file_stat.st_mode) && strstr(entry->d_name, ".txt") != NULL)
        {
            // Letzte Änderungszeit
            modified_time = localtime(&file_stat.st_mtime);
            strftime(modified_time_str, sizeof(modified_time_str), "%Y-%m-%d %H:%M:%S", modified_time);
            sprintf(msgBuffer, "%s : %s %lld",entry->d_name,  modified_time_str, (long long)file_stat.st_size);
            numFiles++;
        }
    }
    sprintf(msgBuffer, "%d Dateien", numFiles);
    // Verzeichnis schließen
    closedir(dir);

    if(send(socketFd, msgBuffer, sizeof(msgBuffer), 0) == -1)
    {
        perror("Error in send");
        exit(EXIT_FAILURE);
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