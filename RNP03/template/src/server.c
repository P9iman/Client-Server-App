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
#include <netdb.h>
#include <dirent.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <libgen.h>

#define DEFAULT_PORT 0
#define BUFFER_SIZE 256
#define HOSTNAME_SIZE 50
#define MAX_CLIENTS 10

void handleList(int socketFd);
void handleFiles(int socketFd);
void handleGet(char *arg, int socketFd);
void handlePut(char *data, int socketFd);
void setnonblocking(int socket);
int getPortFromConnectedClient(int fd);
void getHostname(struct sockaddr *addr, char *hostname, int hostnameLen); 
void get_port_and_ip_server(int socketFd, char *ipAddress, int *port);
void get_port_and_ip_client(int socketFd, char *ipAddress, int *port); 
void get_port_and_ip_helper(struct sockaddr_storage addr ,char *ipAddress, int *port); 

int filesCounter; /*Diese Variable ist dazu da um die Anzahl an Files auf dem Server zu tracken*/
char serverHostname[HOSTNAME_SIZE];  
int sfd_listener; /*sfd_listener = Socket-File-Diskreptor für listening*/
fd_set read_fdset; /*in diesem Set mit File-Discreptoren sind die fds für die Sockets aus denen gelesen werden soll*/ 
int maxFd; /*der größte FD, wird für select benötigt*/
char msgBuffer[BUFFER_SIZE]; /*Buffer für die Nachricht des Clienten*/

/*Struct um beim List Request einfach die Clienten zurückzugeben*/
typedef struct{
    char hostname[HOSTNAME_SIZE];
    int port;
    int socketFd; //wenn kein client in dem arrayslot verbunden ist, ist socketFd = -1 
}ClientInfo;
ClientInfo connectedClients[MAX_CLIENTS]; 
int numConnectedClients = 0;  


void setnonblocking(int socket)
{
    int opts;
    opts = fcntl(socket,F_GETFL);
    if (opts < 0) {
        perror("fcntl(F_GETFL) in setnonblocking()");
        exit(EXIT_FAILURE);
    }
    opts = (opts | O_NONBLOCK);
    if (fcntl(socket,F_SETFL,opts) < 0){
        perror("fcntl(F_SETFL) in setnonblocking()");
        exit(EXIT_FAILURE);
    }
}

void buildSelectList()
{
    //jedes mal nach select muss das fdset neu initialisiert werden
    FD_ZERO(&read_fdset); 
    //auch der listener Fd-Socket muss immer wieder eingefügt werden da er nach select nicht gesetzt ist
    FD_SET(sfd_listener, &read_fdset);
    for(int i = 0; i < MAX_CLIENTS; i++)
    {
        if(connectedClients[i].socketFd > 0){
            FD_SET(connectedClients[i].socketFd, &read_fdset); 
        }
        if(connectedClients[i].socketFd > maxFd){
            maxFd = connectedClients[i].socketFd; 
        }
    }
}

void handleNewConnection()
{
    struct sockaddr_storage sa_client;
    socklen_t sa_len = sizeof(struct sockaddr_storage);
    int newConnection;
    ssize_t sendRetVal;
    int clientPort; 
    char clientIPAddress[INET6_ADDRSTRLEN]; /*Die IPv4 oder IPv6 Adresse vom client*/
    char clientHostname[HOSTNAME_SIZE];
    memset(clientIPAddress,0, sizeof(clientIPAddress));
    memset(clientHostname, 0, sizeof(clientHostname));
    newConnection = accept(sfd_listener, (struct sockaddr *)&sa_client, &sa_len);
    if(newConnection < 0)
    {
        perror("accept in handleNewConnection()");
        exit(EXIT_FAILURE);
    }
    if(numConnectedClients == MAX_CLIENTS)
    {
        /* Kein Platz mehr im Server benachrichtige den client mit einer Nachricht der Größe 0*/
        send(newConnection, "", 0, 0);
        close(newConnection);
        return;
    }else{
        sendRetVal = send(newConnection, "Connected", sizeof("Connected"), 0); 
    }
    if(sendRetVal == -1){
        close(newConnection); 
        perror("send in handleNewConnection()");
        exit(EXIT_FAILURE);  
    }
    //setnonblocking(newConnection);

    get_port_and_ip_client(newConnection, clientIPAddress, &clientPort); 
    printf("Client with address %s has connected on port %d\n", clientIPAddress, clientPort);
    getHostname((struct sockaddr*)&sa_client, clientHostname, HOSTNAME_SIZE);

	//char clientHostname[] = "LAB33";
    // Speichern die Client-Daten in der Datenstruktur
    ClientInfo clientInfo;
    snprintf(clientInfo.hostname, HOSTNAME_SIZE, "%s", clientHostname);
    clientInfo.port = clientPort;  
    clientInfo.socketFd = newConnection; 
    //suche einen freien Platz in der Liste von connected clients und speichere die neue Verbindung
    connectedClients[numConnectedClients] = clientInfo;
    numConnectedClients++;
}

void dealWithData(int socketFd)
{
    char dataBuffer[BUFFER_SIZE];
    memset(dataBuffer, 0, BUFFER_SIZE);
    ssize_t recvRetVal;
    if ((recvRetVal = recv(socketFd, dataBuffer, sizeof(dataBuffer), 0)) > 0)
    {
        printf("Received data from client on port %d: %s\n", getPortFromConnectedClient(socketFd), dataBuffer);
        if(strncmp("Get ", dataBuffer, 4) == 0)
        {
            handleGet(dataBuffer, socketFd);
        }else
        if(strncmp("Put ", dataBuffer, 4) == 0)
        {
            handlePut(dataBuffer, socketFd);
        }else
        if(strncmp("Files", dataBuffer, 5) == 0)
        {
            handleFiles(socketFd);
        }else
        if(strncmp("List", dataBuffer, 4) == 0)
        {
            handleList(socketFd);
        }
    }else
    if(recvRetVal == 0)
    {
        /*Wir haben als Return Value von recv() 0 erhalten, das bedeutet ein Client hat die Verbindung zum Server geschlossen*/
        int port = getPortFromConnectedClient(socketFd); 
        printf("Client on Port %d has disconnected... Closing Port %d\n",port,port);
        close(socketFd);
        FD_CLR(socketFd, &read_fdset); //Lösche den fd aus dem Set
        //Entferne (bzw. Markiere) den client als disconnected damit ein Platz im Array frei wird
        for(int i = 0; i < MAX_CLIENTS; i++){
            if(connectedClients[i].socketFd == socketFd)
            {
                memset(connectedClients[i].hostname, 0, INET6_ADDRSTRLEN);
                connectedClients[i].socketFd = 0; 
                connectedClients[i].port = 0;  
            }
        }
        numConnectedClients--; 
    }else
    {
        //Fehler beim Empfangen der Nachricht
        perror("recv in dealWithData");
        close(socketFd);
        exit(EXIT_FAILURE);
    }
}

void readSockets()
{
    if(FD_ISSET(sfd_listener, &read_fdset))
    {
        handleNewConnection();
    }
    for(int i = 0; i < MAX_CLIENTS; i++)
    { 
        if((connectedClients[i].socketFd != 0) && FD_ISSET(connectedClients[i].socketFd, &read_fdset))
        {
            dealWithData(connectedClients[i].socketFd);
        }
    }
}


int main(int argc, char** argv)
{
    int yes = 1; // for setsockopt() SO_REUSEADDR, below
    int selectRetVal; /*Hier drinn steht der RetVal von select, also die Anzahl an readable Sockets*/
    char serverIPAddress[INET6_ADDRSTRLEN];
    char* serverPort; /*Über diesen Port hört der Server auf neue Verbindungsanfragen*/  
	memset(msgBuffer, 0, BUFFER_SIZE);
	/*Konfiguriere Socket für getaddrinfo() Funktion*/
    int getddrinfoRetVal;
	struct addrinfo *p;			  //Index vom aktuelle Node der verketten Liste
	struct addrinfo hints; 		  //in diesem Struct steht wie der Socket konfiguriert werden soll
  	struct addrinfo *serverInfo;  //Pointer auf eine Verkettet Liste nach dem getaddrinfo() Aufruf
  	memset(&hints, 0, sizeof hints); 
  	hints.ai_family = AF_UNSPEC; //Adress-Family ist unspezifiziert somit IPv4 und IPv6 möglich
	hints.ai_socktype = SOCK_STREAM; 
	//hints.ai_flags = AI_PASSIVE; //befüllt die IP Adresse fuer mich
    hints.ai_protocol = IPPROTO_TCP; 

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

	/**
	 * Als hostname wird hier NULL übergeben, abhängig von den ai_flags
	 * wird die IP-Adresse 0.0.0.0 oder 127.0.0.1 verwendet.
	*/
                                        //HIER AENDERUNG
	if((getddrinfoRetVal = getaddrinfo("localhost",serverPort, &hints, &serverInfo)) != 0)
    {
		fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(getddrinfoRetVal));
		return 1; 
	}
    
    for(p = serverInfo; p != NULL; p = p->ai_next)
	{
		sfd_listener = socket(p->ai_family, p->ai_socktype, 0);
		if(sfd_listener < 0)
		{
			continue;
		}
        //damit die Erro-Meldung: "address already in use" nicht vorkommet
        setsockopt(sfd_listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
        setnonblocking(sfd_listener);
		if(bind(sfd_listener, (struct sockaddr*)p->ai_addr, p->ai_addrlen) == 0)
		{            
            //Rufe den Serhostname mit getnameinfo ab
            getHostname(serverInfo->ai_addr, serverHostname, HOSTNAME_SIZE);
            //Der fd sfd_listener konnte erfolgreich mit der Socketadresse gebunden werden
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
	if(listen(sfd_listener, 5) < 0) 
	{
		perror("listen");
		if(close(sfd_listener) < 0)
		{
			perror("close in main()"); 
		}
		return EXIT_FAILURE;
	}

    int port; 
    get_port_and_ip_server(sfd_listener, serverIPAddress, &port); 
    printf("Serverhostname: %s\n", serverHostname);
    printf("Server local IP Adresse: %s\nServer listening on Port: %d\n\n", serverIPAddress, port);
    printf("Waiting for TCP connections ... \n");
    maxFd = sfd_listener; /*der letzte fd ist der größte somit sfd_listener*/

    /**
     *  Iteriere über die Menge der FD um entweder auf neue Verbindungen zu reagieren (also wenn sich der fd Zustand von sfd_listener verändert hat)
     *  oder wenn einer der Socket FD für communication ready ist, darauf zu reagieren um Nachrichten (mit recv) zu erhalten.
     */
    while (1)
	{
        buildSelectList();

        selectRetVal = select(FD_SETSIZE, &read_fdset, NULL, NULL, NULL);
        if(selectRetVal == -1)
        {
            perror("select in main()");
            return EXIT_FAILURE;
        }
        if(selectRetVal == 0)
        {
            //kein Socket ist ready für read. Zeige einfach das der Server noch läuft.
            printf(".");
            //fflush(stdout);
        }else
        {
            readSockets();
        }
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
void handleGet(char *data, int socketFd)
{
    //Parse den Filename
	char* filename = strtok(data + 4, " ");
	FILE *file = fopen(filename, "r");
	if(file == NULL)
	{
		perror("fopen in handleGet()");
        exit(EXIT_FAILURE);
	}
	int fseekRetVal = fseek(file, 0, SEEK_END);
    if(fseekRetVal != 0)
    {
        perror("fseek in handleGet()");
        exit(EXIT_FAILURE);
    }

	long fileSize = ftell(file);
    if(fileSize == -1){
        perror("ftell in handleGet()");
        exit(EXIT_FAILURE);
    }

    fseekRetVal = fseek(file, 0, SEEK_SET);
    if(fseekRetVal != 0)
	{
		perror("fseek in handleGet()");
        exit(EXIT_FAILURE);
	} 

	char* fileData = (char*)malloc(fileSize); 
	if(fileData == NULL)
	{
		perror("malloc in handleGet()"); 
		if(fclose(file) != 0)
		{
			perror("fclose nach malloc in handleGet()"); 
		}
        exit(EXIT_FAILURE);
	}

	size_t bytesRead = fread(fileData, sizeof(char), fileSize, file); 
	if(bytesRead != (size_t)fileSize)
	{
		perror("fread in handleGet()"); 
		if(fclose(file) != 0)
		{
			perror("fclose in handleGet()"); 
		} 
		free(fileData);
        exit(EXIT_FAILURE);
	}
	if(fclose(file) != 0)
	{
		perror("fclose in handleGet()");
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
		perror("malloc für *response in handleGet()");
        exit(EXIT_FAILURE);
	}
	sprintf(response, "Datei: %s\nDateigröße: %ld\nLetzte Änderung: %s\n\n%s", filename, fileSize, formattedTime, fileData); 
	if (send(socketFd, response, strlen(response), 0) == -1)
    {
        perror("send in handleGet()");
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
void handlePut(char *data, int socketFd)
{
    /*===== Als erstes erhält der Server nur den Befehl und den Filenamen =====*/
    printf("Received Put-Request: %s\n", data);
    char command[10];
    char filepath[100];
    char filecontent[BUFFER_SIZE];
    memset(filecontent, 0, sizeof(filecontent));
    memset(filepath, 0, sizeof(filepath));
    sscanf(data, "%s %s", command, filepath);
    //Parse nur den Filename und entferne den Pfad, da sonst fopen fehlschlägt
    char *filename = basename(filepath);
    printf("Command: %s\n", command);
    printf("Filename: %s\n", filename);
    /*===== Öffne ein File mit dem erhaltenen Filenamen =====*/
    FILE *file = fopen(filename, "a");
    if(file == NULL){
        send(socketFd, "NACK", sizeof("NACK"),0);
        perror("fopen in handlePut");
        exit(EXIT_FAILURE);
    }
    /*===== Schicke ein ACK, damit der Client weiß das filename korrekt übertragen wurde =====*/
    char ack[] = "ACK";
    ssize_t byteSent = send(socketFd, ack, strlen(ack), 0);
    if(byteSent == -1){
        perror("send in handlePut");
        fclose(file);
        exit(EXIT_FAILURE);
    }

    /*===== File konnte geöffnet werden, warte nun auf den Fileinhalt und schreib ihn das geöffnete File =====*/
    /*===== Schreibe so lange in das File bis das EOT beim Server ankommt =====*/
    ssize_t recvRet;
    //char printfBuffer[BUFFER_SIZE];
    //memset(printfBuffer, 0, BUFFER_SIZE);
    //int retSnprintf;
    while((recvRet = recv(socketFd, filecontent, sizeof(msgBuffer), 0)) > 0)
    {
        /*===== Bestätige dem client den Erhalt des EOT Zeichens, damit wird Datenübertragun beendet =====*/
        if(recvRet == 1 && filecontent[0] == '\x04'){
            char eotRecvResponse[] = "EOT received. Ending transmission\n";
            byteSent = send(socketFd, eotRecvResponse, sizeof(eotRecvResponse), 0);
            if(byteSent == -1){
                perror("send in handlePut");
                fclose(file);
                exit(EXIT_FAILURE);
            }
            break;
        }
        //memset(printfBuffer, 0, BUFFER_SIZE);
        //retSnprintf = snprintf(printfBuffer, sizeof(printfBuffer), "%s", filecontent);
        //fprintf(file, "%s", printfBuffer);
        fprintf(file, "%s", filecontent);
        /*===== Schicke ACK oder NACK um den Erhalt des ersten Datenblocks zu bestätigen =====*/
/*
        if(retSnprintf == -1 || retSnprintf > sizeof(printfBuffer))
        {
            char nack[] = "NACK";
            byteSent = send(socketFd, nack, strlen(nack), 0);
            if(byteSent == -1){
                perror("send in handlePut");
                fclose(file);
                exit(EXIT_FAILURE);
            }
        }
*/
        printf("File Content (geprintet in das File):\n%s\n", filecontent);
        //Man könnte bei Erfolg von fprintf ein ACK schicken, aber ist nicht so wichtig
        memset(filecontent, 0, sizeof(filecontent));
    }
    if(recvRet == -1){
        perror("recv in handlePut, recv in while-loop");
        exit(EXIT_FAILURE);
    }
    fclose(file);
	printf("Datei lokal auf den Server gespeichert.\n");
    //erhöhe den Filescounter um den Befehl Files aktuell zu halten.
    filesCounter++;

    /*==== Generiere Responsenachricht und schicke erfolgreiche  Speicherung des Files ====*/
	char response[185];
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
	snprintf(response, sizeof(response), " OK %s,\n Server IP: %s,\n Date: %s %s\n\n", serverHostname, serverIP, __DATE__, timerString);
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
    int counterConnectedClients = 0;
    strcpy(msgBuffer, "");

    for(int i = 0; i < MAX_CLIENTS; i++)
    {
        if(connectedClients[i].socketFd != 0)
        {
            char clientInfo[BUFFER_SIZE];
            sprintf(clientInfo, "%s : %d\n",connectedClients[i].hostname, connectedClients[i].port);
            strcat(msgBuffer, clientInfo);
            counterConnectedClients++;
        }
    }
    sprintf(msgBuffer + strlen(msgBuffer),"%d Client[s] verbunden\n", counterConnectedClients);
    if(send(socketFd, msgBuffer, strlen(msgBuffer), 0) == -1)
    {
        perror("send in handleList");
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
    if(filesCounter == 0){
        sprintf(msgBuffer, "%d Datei[en]\n", filesCounter);
        if(send(socketFd, msgBuffer, sizeof(msgBuffer), 0) == -1)
        {
            perror("send in handleFiles");
            exit(EXIT_FAILURE);
        }
    }else
    {
        char directory[] = ".";
        DIR *dir;
        struct dirent *entry;
        struct stat file_stat;
        struct tm *modified_time;
        char modified_time_str[20];
        int numFiles = 0;

        // Verzeichnis öffnen
        dir = opendir(directory);
        if (dir == NULL) {
            perror("opendir in handleFiles()");
            exit(EXIT_FAILURE);
        }
        // Alle Einträge im Verzeichnis durchlaufen
        while ((entry = readdir(dir)) != NULL)
        {
            // Kompletten Dateipfad erstellen
            char file_path[PATH_MAX];
            //sprintf(file_path, "%s/%s", directory, entry->d_name);
            snprintf(file_path, sizeof(file_path), "%s/%s", directory, entry->d_name);

            // Dateiattribute abrufen
            if (stat(file_path, &file_stat) == -1)
            {
                perror("stat in handleFiles");
                exit(EXIT_FAILURE);
            }

            // Nur reguläre Dateien berücksichtigen
            if (S_ISREG(file_stat.st_mode) && strstr(entry->d_name, ".txt") != NULL)
            {
                // Letzte Änderungszeit
                modified_time = localtime(&file_stat.st_mtime);
                strftime(modified_time_str, sizeof(modified_time_str), "%Y-%m-%d %H:%M:%S", modified_time);
                strcat(msgBuffer, entry->d_name);
                strcat(msgBuffer, " : ");
                strcat(msgBuffer, modified_time_str);
                strcat(msgBuffer, " ");
                sprintf(msgBuffer + strlen(msgBuffer), "%lld\n", (long long)file_stat.st_size);
                numFiles++;
            }
        }
        sprintf(msgBuffer + strlen(msgBuffer), "%d Datei[en]\n", numFiles);
        // Verzeichnis schließen
        closedir(dir);

        if(send(socketFd, msgBuffer, sizeof(msgBuffer), 0) == -1)
        {
            perror("send in handleFiles()");
            exit(EXIT_FAILURE);
        }
    }

}

/**
 * @brief Diese Hilfsfunktion dient dazu den Hostnamen abzurufen und in den Buffer char* hostname abzuspeichern.
*/
void getHostname(struct sockaddr *addr, char *hostname, int hostnameLen) 
{
    int getnameinfoRetVal; 
    socklen_t addrLen = (addr->sa_family == AF_INET) ?  INET_ADDRSTRLEN : INET6_ADDRSTRLEN;
    getnameinfoRetVal = getnameinfo(addr, addrLen, hostname, hostnameLen, NULL, 0, 0);
    if (getnameinfoRetVal != 0) {
        fprintf(stderr, "error in getnameinfo: %s\n", gai_strerror(getnameinfoRetVal)), exit(EXIT_FAILURE);
    }
    if(*serverHostname == '\0')
    {
        printf("Hostname couldnt be determined\nSetting numeric form\n");
    }
}

void get_port_and_ip_client(int socketFd, char *ipAddress, int *port)
{
    struct sockaddr_storage addr; 
    socklen_t len = sizeof(addr); 
    //einzige Unterschied zwischen get_port_and_ip_client und get_port_and_ip_server ist das get_port_and_ip_client getpeername benutzt
    if(getpeername(socketFd, (struct sockaddr*)&addr, &len) == -1)
    {
        perror("getpeername in get_port_and_ip_client"); 
        exit(EXIT_FAILURE); 
    }    
    get_port_and_ip_helper(addr, ipAddress, port); 
}

void get_port_and_ip_server(int socketFd, char *ipAddress, int *port)
{
    struct sockaddr_storage addr; 
    socklen_t len = sizeof(addr); 
    //einzige Unterschied zwischen get_port_and_ip_client und get_port_and_ip_server ist das get_port_and_ip_server getsockname benutzt
    if (getsockname(socketFd, (struct sockaddr *)&addr, &len) == -1) 
    {
        perror("getsockname in get_port_and_ip_server");
        exit(EXIT_FAILURE); 
    }
    get_port_and_ip_helper(addr, ipAddress, port); 
}

void get_port_and_ip_helper(struct sockaddr_storage addr ,char *ipAddress, int *port)
{
     if(addr.ss_family == AF_INET){
        //IPv4-Fall
        struct sockaddr_in *ipv4 = (struct sockaddr_in *)&addr;
        *port = ntohs(ipv4->sin_port); 
        inet_ntop(AF_INET, &(ipv4->sin_addr), ipAddress, INET_ADDRSTRLEN);
    }else
    if(addr.ss_family == AF_INET6){
        //IPv6-Fall
        struct sockaddr_in6 *ipv6 = (struct sockaddr_in6 *)&addr; 
        *port = ntohs(ipv6->sin6_port); 
        inet_ntop(AF_INET6, &(ipv6->sin6_addr), ipAddress, INET6_ADDRSTRLEN);
    }else
    {
        printf("\nUnknown client address ....\n"); 
        exit(EXIT_FAILURE); 
    }
}

int getPortFromConnectedClient(int fd)
{
    int counter = 0;  
    while(connectedClients[counter].socketFd != fd)
        counter++;
    return connectedClients[counter].port;
}
