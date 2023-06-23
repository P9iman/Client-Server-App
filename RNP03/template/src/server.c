#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>

#include <arpa/inet.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>

#define DEFAULT_PORT 0
#define BUFFER_SIZE 256
#define HOSTNAME_SIZE 50
#define MAX_CLIENTS 10
#define FILENAME_SIZE 100


/**
 * @file server.c
 * @brief Ein Server-Modul zur Verwaltung von Dateien und Bereitstellung von Dateioperationen für Clients.
 *
 * Das Server-Modul ermöglicht es Clients, sich mit dem Server zu verbinden und verschiedene Dateioperationen
 * durchzuführen, wie das Auflisten von Dateien, das Herunterladen von Dateien, das Hochladen von Dateien usw.
 * Der Server arbeitet auf nicht blockierende Weise und kann gleichzeitig mehrere Clients bedienen.
 * Das Modul stellt Funktionen bereit, um Anfragen von Clients zu verarbeiten und entsprechende Aktionen auszuführen.
 *
 * Die wichtigsten Funktionen des Moduls sind:
 * - handleList: Behandelt die Anforderung des List-Befehls vom Client.
 * - handleFiles: Behandelt die Anforderung des Files-Befehls vom Client.
 * - handleGet: Behandelt die Anforderung des Get-Befehls vom Client.
 * - handlePut: Behandelt die Anforderung des Put-Befehls vom Client.
 * - getPortFromConnectedClient: Ruft den Port von dem verbundenen Client ab.
 * - getHostname: Ruft den Hostnamen von der Socket-Adressstruktur ab.
 * - get_port_and_ip_client: Ruft die IP-Adresse und den Port von dem Client-Socket ab.
 *
 * @note
 * Bevor der Server gestartet wird sollte im bin Ordner ein Verzeichnis namens "ServerData" erstellt werden, da 
 * die Dateien die der Server durch Put erhält dort speichert und bei einem Get dort aufsucht. 
 * 
 */




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

/*Struct um beim List Request einfach die Clienten zurückzugeben*/
typedef struct{
    char hostname[HOSTNAME_SIZE];
    int port;
    int socketFd; 
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
        printf("Received data from client on port %d\n", getPortFromConnectedClient(socketFd));
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
        return;
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
    char msgBuffer[BUFFER_SIZE]; /*Buffer für die Nachricht des Clienten*/
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
	hints.ai_flags = AI_PASSIVE; //befüllt die IP Adresse fuer mich
    //hints.ai_protocol = IPPROTO_TCP; 

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
 * Behandelt die Anforderung des Get-Befehls vom Client.
 *
 * @param data      Die Get-Nachricht, die vom Client erhalten wurde.
 * @param socketFd  Die Socket-Dateideskriptor.
 */
void handleGet(char *data, int socketFd)
{
    /*===== Als erstes erhält der Server nur den Befehl und den Filenamen =====*/
    printf("Received Get-Requst: %s\n", data);
    char command[10];
    char filename[FILENAME_SIZE];
    char filecontent[BUFFER_SIZE];
    char serverDataDir[] = "ServerData";
    memset(filename, 0, sizeof(filename));
    memset(filecontent, 0, sizeof(filecontent));
    memset(command, 0, sizeof(command));
    sscanf(data, "%s %s", command, filename);
    printf("Filename: %s\n", filename);
    /*===== Versuche die Datei in ServerData zu finden und zu öffnen =====*/
    char filepathinServerData[FILENAME_MAX];
    //snprintf(filepathinServerData, sizeof(filepathinServerData), "%s/%s", serverDataDir, filename);
    sprintf(filepathinServerData, "%s/%s", serverDataDir, filename);
    FILE *file = fopen(filepathinServerData, "r");
    /*===== Sende NACK an client, File konnte nicht geöffnet werden =====*/
    if(file == NULL){
        send(socketFd, "NACK, File not found!", sizeof("NACK, File not found!"),0);
        perror("fopen in handlePut");
        return;
    }
    /*===== Sende ACK an client, File konnte geöffnet werden =====*/
    ssize_t recvRet;
    ssize_t byteSent;
    byteSent =  send(socketFd, "ACK", sizeof("ACK"), 0);
    if(byteSent == -1){
        perror("send in handleGet, while sending ACK to client");
        fclose(file);
        exit(EXIT_FAILURE);
    }

    /*===== Warte auf das Ready Signal vom client um danach mit Übertragung von Datei-Attribute zu beginnen =====*/
    char ready[10];
    memset(ready, 0, sizeof(ready));
    recvRet = recv(socketFd, ready, sizeof(ready), 0);
    if(recvRet == -1){
        perror("recv in handleGet, while waiting for client response if ready for data-attributes");
        fclose(file);
        exit(EXIT_FAILURE);
    }

    /*===== Sende Datei-Attribute an Client  =====*/
    struct stat fileStat;
    if(stat(filepathinServerData, &fileStat) == -1){
        perror("stat in handleGet");
        fclose(file);
        exit(EXIT_FAILURE);
    }
    char attributeMsg[BUFFER_SIZE];
    memset(attributeMsg, 0, sizeof(attributeMsg));
    time_t modifiedTime = fileStat.st_mtim.tv_sec;
    struct tm* modifiedTimeInfo = localtime(&modifiedTime);
    char modifiedTimeString[100];
    strftime(modifiedTimeString, sizeof(modifiedTimeString), "%d-%m-%Y %H:%M:%S", modifiedTimeInfo);
    snprintf(attributeMsg, sizeof(attributeMsg), "Datei-Attribute: last modified: %s, size: %ld Bytes", modifiedTimeString, fileStat.st_size);

    byteSent = send(socketFd, attributeMsg, sizeof(attributeMsg), 0);
    if(byteSent == -1){
        perror("send in handleGet, while sending file attributes");
        fclose(file);
        exit(EXIT_FAILURE);
    }
    /*===== Warte auf ACK vom Client =====*/
    char ack[10];
    memset(ack, 0, sizeof(ack));
    recvRet = recv(socketFd, ack, sizeof(ack), 0);
    if (recvRet == -1) {
        perror("recv in handleGet, while waiting for ACK for file attributes");
        fclose(file);
        exit(EXIT_FAILURE);
    }
    if (strcmp(ack, "ACK") != 0) {
        printf("No ACK from client received! File attributes were not acknowledged.\n");
        fclose(file);
        return;
    }
    /*===== Beginne mit der Übertragung des Dateiinhalts =====*/
    size_t bytesRead = 1;
    while(1){
        bytesRead = fread(filecontent, 1, BUFFER_SIZE-2, file);
        if(ferror(file) != 0){
            printf("Error reading file: %s\n", filename);
            clearerr(file);
            fclose(file);
            exit(EXIT_FAILURE);
        }
        byteSent = send(socketFd, filecontent, bytesRead,0);
        if(byteSent == -1){
                perror("send in sendPutRequest while sending filecontent to client");
                fclose(file);
                exit(EXIT_FAILURE);
        }
        /*===== warte nach jeder Übertragung ein Datenblocks auf ein ACK um danach weiter zu machen=====*/
        memset(ack, 0, sizeof(ack));
        recvRet = recv(socketFd, ack, sizeof(ack), 0);
        if(recvRet == -1){
            perror("recv in handleGet, while waiting for ACK recv datablock");
            fclose(file);
            exit(EXIT_FAILURE);
        }
        if(strcmp(ack, "NACK") == 0){
            printf("No ACK from client received! Filecontent was not received\n");
            exit(EXIT_FAILURE);
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
        perror("send in handleGet, while sending eot");
        fclose(file);
        exit(EXIT_FAILURE);
    }
}

/**
 * Behandelt die Anforderung des Put-Befehls vom Client.
 *
 * @param data      Die Put-Nachricht, die vom Client erhalten wurden.
 * @param socketFd  Die Socket-Dateideskriptor.
 */
void handlePut(char *data, int socketFd)
{
    /*===== Als erstes erhält der Server nur den Befehl und den Filenamen =====*/
    printf("Received Put-Request: %s\n", data);
    char command[10];
    char filepath[100];
    char serverDataDir[] = "ServerData";
    char filecontent[BUFFER_SIZE];
    memset(filecontent, 0, sizeof(filecontent));
    memset(filepath, 0, sizeof(filepath));
    sscanf(data, "%s %s", command, filepath);
    //Parse nur den Filename und entferne den Pfad, da sonst fopen fehlschlägt
    char *filename = basename(filepath);
    printf("Filename: %s\n", filename);
    /*===== Öffne ein File mit dem erhaltenen Filenamen im Verzeichnis "ServerData" =====*/
    char filepathinServerData[FILENAME_MAX];
    sprintf(filepathinServerData, "%s/%s", serverDataDir, filename);

    //snprintf(filepathinServerData, sizeof(filepathinServerData), "%s/%s", serverDataDir, filename);
    FILE *file = fopen(filepathinServerData, "a");
    int retprintf;
    if(file == NULL){
        send(socketFd, "NACK", sizeof("NACK"),0);
        perror("fopen in handlePut");
        exit(EXIT_FAILURE);
    }
    /*===== Sende ACK an client das filename und Befehl korrekt erhalten wurde =====*/
    ssize_t recvRet;
    ssize_t byteSent;
    byteSent =  send(socketFd, "ACK", sizeof("ACK"), 0);
    if(byteSent == -1){
        perror("send in handlePut, while sending ACK to client");
        fclose(file);
        exit(EXIT_FAILURE);
    }
    /*===== File konnte geöffnet werden, warte nun auf den Fileinhalt und schreib ihn das geöffnete File =====*/
    /*===== Schreibe so lange in das File bis das EOT beim Server ankommt =====*/
    while(1)
    {
        memset(filecontent, 0, sizeof(filecontent));
        recvRet = recv(socketFd, filecontent, sizeof(filecontent), 0);

        /*===== Bestätige dem client den Erhalt des EOT Zeichens, damit wird Datenübertragun beendet =====*/
        if(recvRet == 1 && filecontent[0] == '\x04'){
            char eotRecvResponse[] = "EOT received\n";
            byteSent = send(socketFd, eotRecvResponse, sizeof(eotRecvResponse), 0);
            if(byteSent == -1){
                perror("send in handlePut, while sending ACK for recv EOT");
                fclose(file);
                exit(EXIT_FAILURE);
            }
            break;
        }
        if(recvRet == -1){
            byteSent = send(socketFd, "NACK", sizeof("NACK"), 0);
            if(byteSent  == -1){
                perror("send in handlePut, while sending NACK to client");
            }
            perror("recv in handlePut, recv in while-loop");
            fclose(file);
            return;
        }
        retprintf = fprintf(file, "%s", filecontent);
        /*===== Schicke ACK oder NACK um den Erhalt des ersten Datenblocks zu bestätigen =====*/
        char ackOrNack[10];
        strncpy(ackOrNack, "ACK", strlen("ACK")+1);
        if(retprintf == -1 || retprintf != recvRet){
            strncpy(ackOrNack, "NACK", strlen("NACK")+1);
        }
        byteSent = send(socketFd, ackOrNack, sizeof(ackOrNack), 0);
        if(byteSent == -1){
            perror("send in handlePut, while sending ack or nack for recv filecontent");
            fclose(file);
            exit(EXIT_FAILURE);
        }
        //Man könnte bei Erfolg von fprintf ein ACK schicken, aber ist nicht so wichtig
        memset(filecontent, 0, sizeof(filecontent));
    }
    fclose(file);
	printf("Datei lokal auf dem Server gespeichert.\n");
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
	byteSent =  send(socketFd, response, sizeof(response), 0);
    if(byteSent == -1){
        perror("send in handlePut, while sending response to client");
        return;
    }
}

/**
 * Behandelt die Anforderung des List-Befehls vom Client.
 *
 * @param socketFd Die Socket-Dateideskriptor.
 */
void handleList(int socketFd)
{
    char buffer[BUFFER_SIZE];
    memset(buffer, 0, sizeof(buffer));
    int counterConnectedClients = 0;
    for(int i = 0; i < MAX_CLIENTS; i++)
    {
        if(connectedClients[i].socketFd != 0)
        {
            char clientInfo[BUFFER_SIZE];
            sprintf(clientInfo, "%s : %d\n",connectedClients[i].hostname, connectedClients[i].port);
            strcat(buffer, clientInfo);
            counterConnectedClients++;
        }
    }
    sprintf(buffer + strlen(buffer),"%d Client[s] verbunden\n", counterConnectedClients);
    if(send(socketFd, buffer, strlen(buffer), 0) == -1)
    {
        perror("send in handleList");
        exit(EXIT_FAILURE);
    }
}

/**
 * Behandelt die Anforderung des Files-Befehls vom Client.
 *
 * @param socketFd Die Socket-Dateideskriptor.
 */
void handleFiles(int socketFd)
{
    char buffer[BUFFER_SIZE];
    memset(buffer, 0, sizeof(buffer));
    if(filesCounter == 0){
        sprintf(buffer, "%d Datei[en]\n", filesCounter);
        if(send(socketFd, buffer, sizeof(buffer), 0) == -1){
            perror("send in handleFiles");
            exit(EXIT_FAILURE);
        }
    }else
    {
        char directory[] = "ServerData";
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
                strcat(buffer, entry->d_name);
                strcat(buffer, " : ");
                strcat(buffer, modified_time_str);
                strcat(buffer, " ");
                sprintf(buffer + strlen(buffer), "%lld\n", (long long)file_stat.st_size);
                numFiles++;
            }
        }
        sprintf(buffer + strlen(buffer), "%d Datei[en]\n", numFiles);
        // Verzeichnis schließen
        closedir(dir);

        if(send(socketFd, buffer, sizeof(buffer), 0) == -1)
        {
            perror("send in handleFiles()");
            exit(EXIT_FAILURE);
        }
    }

}

/**
 * Ruft den Hostnamen von der Socket-Adressstruktur ab.
 *
 * @param addr        Die Socket-Adressstruktur.
 * @param hostname    Der Puffer zum Speichern des Hostnamens.
 * @param hostnameLen Die Länge des Hostnamen-Puffers.
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

/**
 * Ruft die IP-Adresse und den Port von dem Client-Socket ab.
 *
 * @param socketFd    Der Socket-Dateideskriptor des Clients.
 * @param ipAddress   Der Puffer zum Speichern der IP-Adresse.
 * @param port        Der Puffer zum Speichern der Portnummer.
 */
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

/**
 * Ruft die IP-Adresse und den Port von dem Server-Socket ab.
 *
 * @param socketFd    Der Socket-Dateideskriptor des Servers.
 * @param ipAddress   Der Puffer zum Speichern der IP-Adresse.
 * @param port        Der Puffer zum Speichern der Portnummer.
 */
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

/**
 * Ruft den Port von dem verbundenen Client ab.
 *
 * @param fd Der Dateideskriptor des verbundenen Clients.
 * @return   Die Portnummer.
 */
int getPortFromConnectedClient(int fd)
{
    int counter = 0;  
    while(connectedClients[counter].socketFd != fd)
        counter++;
    return connectedClients[counter].port;
}
