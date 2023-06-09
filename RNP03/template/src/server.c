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

#define DEFAULT_PORT 0
#define BUFFER_SIZE 256
#define HOSTNAME_SIZE 50

#define EOT '\x04'

/*Prototypen*/

void handleList(int socketFd, int maxFd);
void handleFiles(int socketFd);
void handleGet(char *arg, int socketFd);
void handlePut(char *arg, int socketFd, int dataLength);
void *get_in_addr(struct sockaddr *sa); 


struct utsname serverHostname;

int main(int argc, char** argv)
{
	//Die IPv4 oder IPv6 Adresse vom client 
	char clientIPAddress[INET6_ADDRSTRLEN]; 
	//Der Port von dem Server über dem Nachrichten ausgetauscht werden 
	char* serverPort;
	//Buffer für die Nachricht des Clienten
	char msgBuffer[BUFFER_SIZE];
	memset(msgBuffer, 0, BUFFER_SIZE); 
	//Return Value vom recv 
	ssize_t recvRetVal;

	//es wird struct sockaddr_storage benutzt weil es besser als struct sockaddr_in ist. Kann sowohl IPv4 als auch IPv6 structure halten
	struct sockaddr_storage sa_client; 
	socklen_t sa_len = sizeof(struct sockaddr_storage); 

	/*Erzeuge FD Set um mit select auf Sockets zu reagieren die bereit für I/O sind*/

	//die größte FD Nummer
	int maxFd; 
	//in diesem Set mit File-Discreptoren sind die fds für die Sockets aus denen gelesen werden soll 
	fd_set read_fdset; 
	fd_set read_fdset_copy; 
	//Initialisiere die beiden fd_set
	FD_ZERO(&read_fdset); 
	FD_ZERO(&read_fdset_copy); 

	/**
	 * sfd_listener = Socket-File-Diskreptor für listening
	 * sfd_communication = Socket-File-Diskreptor für Kommunikation mit clients 
	 * sfd_ready = Socket-File-Diskreptor der ready ist für Nachrichtenaustausch
	*/
	int sfd_listener, sfd_communication;


	//TODO: Lieber getnameinfo benutzten 
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


	//TODO: Iteriere über die LinkedList serverInfo um zu prüfen das getaddrinfo 
	//		die Struct und andere Elemente korrekt gefüllt hat. 
	for(p = serverInfo; p != NULL; p = p->ai_next)
	{
		sfd_listener = socket(serverInfo->ai_family, serverInfo->ai_socktype, serverInfo->ai_protocol);
		if(sfd_listener < 0)
		{
			continue;
		}
		/*Verbinde den erzeugten Socket mit einem Port*/

		if(bind(sfd_listener, (struct sockaddr*)serverInfo->ai_addr, serverInfo->ai_addrlen) == 0)
		{
            //Der fd sfd_listener konnte erfolgreich mit der Socketadresse gebunden werden
			break;
		}else
        {
            close(sfd_listener);
        }
	}
    //Die Liste mit Adresse wird nicht mehr benötigt
    freeaddrinfo(serverInfo);
    //Keine Adresse konnte verwendet werden
	if(p == NULL)
	{
		fprintf(stderr, "selectserver: failed to bind\n");
		return 1; 
	}

	/* Warte mit listen() auf eingehende Verbindungen am erzeugten Socket */

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
		return 1;
	}

	// TODO: Check port in use and print it.
	printf("Waiting for TCP connections ... \n");

	//fuege den Socket fd in das read_fdset
	FD_SET(sfd_listener, &read_fdset); 
	//der letzte fd ist der größte somit sfd_listener
	maxFd = sfd_listener; 

	while (1) 
	{
		//jedes Mal wenn Select aufgerufen wird das set aktualsiert, damit die Verbindungen nicht überschrieben werden muss das set zwischengespeichert werden  
		read_fdset_copy = read_fdset; 
 		/**
		 * 1. Arg = nfds, der größte fd+1 
		 * 2. Arg = fd set mit Sockets aus den gelesen werden soll
		*/
		if(select(maxFd + 1, &read_fdset_copy, NULL, NULL, NULL) == -1)
		{
			perror("Fehler bei select()"); 
			return 1; 
		}

		/**
		 * Iteriere über die Menge der FD um entweder auf neue Verbindungen zu reagieren (also wenn sich der fd Zustand von sfd_listener verändert hat)
		 * oder wenn einer der Socket FD für communication ready ist, darauf zu reagieren um Nachrichten (mit recv) zu erhalten.  
		*/
		for(int i = 0; i < maxFd; i++)
		{
			if(FD_ISSET(i, &read_fdset_copy))
			{
				// Es gibt eine neue Verbindungsanfrage
				if(i == sfd_listener)
				{
					/*Akzeptiere Verbindungsanfrage der Clients*/
					/**
				 	* 1. Argument: wir akzeptieren Verbindungen an dem listener Socket-FD
					* 2. Argument: In diesem struct steht die Informationen über die eingehnde Verbindung (Client-Infos)
		 			* 3. Argument: accept() soll maximal sa_len Bytes in sa_client einfügen
					*/

					//neue Verbindung damit auch neuer Socket-FD
					sfd_communication = accept(sfd_listener, (struct sockaddr *)&sa_client, &sa_len); 
					if(sfd_communication < 0)
					{
						perror("accept");
					}else
					{
						//Füge den Socket-FD in die Menge um auf eingehnde Nachrichten zu reagieren
						FD_SET(sfd_communication, &read_fdset); 
						//Aktualisiere den maxFD damit bei der nächsten Iteration wieder alle FDs geprüft werden 
						if(sfd_communication > maxFd)
						{
							maxFd = sfd_communication; 
						}
						/**
						 * printen die IP-Adresse mittels inet_ntop von dem Clienten um sicher zugehen das, das accept erfolgreich war.
						*/
                        char const *inet_ntop_retVal;
                        inet_ntop_retVal = inet_ntop(sa_client.ss_family, get_in_addr((struct sockaddr *) &sa_client),
                                                     clientIPAddress, INET6_ADDRSTRLEN);
						inet_ntop_retVal == NULL ? perror("inet_ntop Fehler beim Konvertieren der Adresse") : 
						printf("Client hat sich verbunden mit dieser Adresse: %s am Socket: %d\n", clientIPAddress, sfd_communication); 
					}
				}else /*Erhalte Nachrichten über den communication Socket*/  
				{
					/**
					 * 1.Arg Lese aus dem Socket sfd_communication Nachrichten
					 * 2.Arg Speichere die Nachrichten in den Buffer msgBuffer
					 * 3.Arg die max. Länge vom Buffer
					 * 4.Arg für Flags
					*/
					if ((recvRetVal = recv(i, msgBuffer, sizeof(msgBuffer), 0)) > 0)
					{
                        //TODO Auf EOT Zeichen achten

						if(strncmp("Get ", msgBuffer, 4) == 0)
						{
							handleGet(msgBuffer, i); 
						}else
						if(strncmp("Put ", msgBuffer, 4) == 0)
						{
							handlePut(msgBuffer, i, sizeof(msgBuffer)); 
						}else
						if(strncmp("Files", msgBuffer, 5) == 0)
						{
							handleFiles(i);
						}else
						if(strncmp("List", msgBuffer, 4) == 0)
						{
							handleList(i, maxFd);
						}
					}else 
					if(recvRetVal == 0)
					{
						/*Wir haben als Return Value von recv() 0 erhalten, das bedeutet ein Client hat die Verbindung zum Server geschlossen*/
						printf("Client am Socket %d hat Verbindung abgebrochen\n", i); 
						printf("Schließe Socket %d\n", i); 
						close(i); 
						FD_CLR(i, &read_fdset); //Lösche den fd aus dem Set
					}else
					{
						//Fehler beim Empfangen der Nachricht
						perror("recv"); 
						close(i);
						return 1; 
					}
				}
			}
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
void handleGet(char *arg, int socketFd)
{
    //Parse den Filename
	char* filename = strtok(arg + 4, " ");
	FILE *file = fopen(filename, "r");
	if(file == NULL)
	{
		perror("Fehler beim Öffnen der Datei");
        exit(1);
	}
	int fseekRetVal = fseek(file, 0, SEEK_END);
    if(fseekRetVal != 0)
    {
        perror("Error in fseek");
        exit(1);
    }

	long fileSize = ftell(file);
    if(fileSize == -1){
        perror("Error in ftell");
    }

    fseekRetVal = fseek(file, 0, SEEK_SET);
    if(fseekRetVal != 0)
	{
		perror("Error in fseek"); 
	} 

	char* fileData = (char*)malloc(fileSize); 
	if(fileData == NULL)
	{
		perror("Fehler bei der Speicherzuweisung"); 
		if(fclose(file) != 0)
		{
			perror("Error in flclose"); 
		}
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
	}
	if(fclose(file) != 0)
	{
		perror("Error in fclose");
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
		free(fileData);   
	}
	sprintf(response, "Datei: %s\nDateigröße: %ld\nLetzte Änderung: %s\n\n%s", filename, fileSize, formattedTime, fileData); 
	if (send(socketFd, response, strlen(response), 0) == -1)
    {
        perror("Fehler beim Senden der Daten");
        free(fileData);
        free(response);
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
	}

	//Dateiinhalt parsen
	char dateiInhalt[BUFFER_SIZE-4]; //252


    strncpy(dateiInhalt, arg + 4, sizeof(dateiInhalt+1)); //arg + 4, damit der Pointer 4 Byte nach rechts geht da die ersten 3 Byte der Befehl Put sind und dann Leerzeichen.


	size_t bytesWritten = fwrite(dateiInhalt, sizeof(char), dataLength-3, file); 
	if (bytesWritten != (size_t)dataLength)
    {
        perror("Fehler beim Schreiben der Daten in die Datei");
        fclose(file);
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
void handleList(int socketFd, int maxFd)
{
    char msgBuffer[BUFFER_SIZE];
    int port;
    char clientHostName[HOSTNAME_SIZE];

    struct sockaddr_storage sa_client;
    socklen_t sa_len = sizeof(struct sockaddr_storage);
    for(int i = 0; i <= maxFd; i++)
    {
        int retVal = getpeername(i, (struct sockaddr*)&sa_client, &sa_len);
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
    char msgBuffer[BUFFER_SIZE];
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
        sprintf(file_path, "%s/%s", directory, entry->d_name);
        //snprintf(file_path, sizeof(file_path), "%s/%s", directory, entry->d_name);

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