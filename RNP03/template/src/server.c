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

#define DEFAULT_PORT 0

void* handleList(char *arg);
void* handleFiles(char *arg);
void handleGet(char *arg, int socketFd);
void handlePut(char *arg, int socketFd, int dataLength);
void* handleQuit(char *arg);


char clientAddress[INET_ADDRSTRLEN]; 
struct utsname serverHostname;

int main(int argc, char** argv)
{
	//TODO: Lieber getnameinfo benutzten 

	//Speicher den Serverhostname in serverHostname ab. 
	if(uname(&serverHostname) == -1) 
	{
        perror("Error in uname");
        return 1;    
    } 

	//Der Port von dem Server über dem Nachrichten ausgetauscht werden 
	char* serverPort;

	if(argc > 2)
	{
		printf("Starting server failed!\nUsage: ./server [Port]\n"); 
      	return 1; 
	}else 
	if(args == 2)
	{
		serverPort = argv[1]; 
	}else
	{
		serverPort = DEFAULT_PORT; 
	}
	
	/*Konfiguriere Socket*/

	/* 
		int getaddrinfo(const char *node,	  // e.g. "www.example.com" or IP
						const char *service, // e.g. "http" or port number
						const struct addrinfo *hints,
						struct addrinfo **res);				
  	*/

	int getddrinfoRetVal; 
	struct addrinfo hints; 
  	struct addrinfo *serverInfo;  
  	memset(&hints, 0, sizeof hints); 
  	hints.ai_family = AF_UNSPEC; //Adress-Family ist unspezifiziert somit IPv4 und IPv6 möglich
	hints.ai_socktype = SOCK_STREAM; 
	hints.ai_flags = AI_PASSIVE; //befüllt die IP Adresse fuer mich
	
	/**
	 * Als hostname wird hier NULL übergeben, abhängig von den ai_flags
	 * wird die IP-Adresse 0.0.0.0 oder 127.0.0.1 verwendet.
	*/
	if((getddrinfoRetVal = getaddrinfo(NULL,serverPort, &hints, serverInfo)) != 0)
	{
		fprintf(stderr, "getaddrinfo error: %s\n", strerror(getddrinfoRetVal));
		return 1; 
	}
	
	//TODO: Iteriere über die LinkedList serverInfo um zu prüfen das getaddrinfo 
	//		die Struct und andere Elemente korrekt gefüllt hat. 


	/*Erzeuge Socket mit socket()*/

	/**
	 * sfd_listener = Socket-Diskreptor für listening
	 * sfd_communication = Socket-Diskreptor für Kommunikation mit clients 
	*/
	int sfd_listener, sfd_communication;

	sfd_listener = socket(serverInfo->ai_family, serverInfo->ai_socktype, serverInfo->ai_protocol); 
	if(sfd_listener < 0)
	{
		perror("TCP Socket");
		return 1;
	}

	////sa = die Socketadresse vom Server 
	// struct sockaddr_in sa;  
	//// Host-to-Networkbyteorder und setze die Portnummer in das struct für den Server
	// sa.sin_port = htons(serverPort);
	// sa.sin_addr.s_addr = INADDR_ANY;
	// if ((sfd_listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) 
	// {
	// 	perror("TCP Socket");
	// 	return 1;
	// }
	//hier Internetadressfamilie = IPv4 
	// sa.sin_family = AF_INET;

	/*Bind den erzeugten Socket mit einem Port*/

	if(bind(sfd_listener, (struct sockaddr*)serverInfo->ai_addr, serverInfo->ai_addrlen) != 0)
	{
		perror("bind");
		return 1;
	}

	/* Warte mit listen() auf eingehende Verbindungen */

	//Nun hört der Server an seinem Socket auf Verbindungsanfrangen
	if (listen(sfd_listener, 5) < 0) 
	{
		perror("listen");
		close(sfd_listener);
		return 1;
	}
	// TODO: Check port in use and print it.
	printf("Waiting for TCP connections ... \n");

	

	/*Ab hier ist der Server im listening Zustand*/

/* 	int retVal = 0; 
	int max_fd = 0;
	//In diesem FD Set werden alle File-Descriptoren gespeichert um auf Veränderungen dieser fd zu reagieren 
	fd_set fd_Set;
	//Initialisieren das fd Set 
	FD_ZERO(&fd_Set);

	//Beispiel für die Anwendung von fd_set 
	int fd1 = 3;
	int fd2 = 5; 
	//Füge fd der Menge an FDs hinzu
	FD_SET(fd1, &fd_Set); 
	FD_SET(fd2, &fd_Set); 

	if (fd1 > max_fd)
        max_fd = fd1;
    if (fd2 > max_fd)
        max_fd = fd2;
 */


	//sa_client = die Socketadresse vom Client
	struct sockaddr_in sa_client;
	char info[256];
	unsigned int sa_len = sizeof(struct sockaddr_in);
	while (1) 
	{
		//TODO Hier select auf das fd_set aufrufen um einen fd auszuwählen der ready ist. 
		/*nfds = der fd der, der größte aus allen fd_sets ist + 1*/
/* 		if((retVal =  select(0, NULL, NULL, NULL, NULL)) == -1)
		{
			perror("Fehler bei select() " __FILE__); 
			return 1; 
		}

		//Überprüfe, welche Dateidiskreptoren bereit sind 
		if(FD_ISSET(fd1, &fd_Set))
		{
			
		}
 */

		//für jede akzeptierte Verbindung wird ein neuer Socket erstellt. In news ist der FD auf diesen neuen Socket
		if ((sfd_communication = accept(sfd_listener, (struct sockaddr*)&sa_client, &sa_len)) < 0) 
		{
			perror("accept");
			close(sfd_listener);
			return 1;
		}

		/*
			printen die IP-Adresse von dem Clienten um sicher zugehen das, das accept erfolgreich war.
			inet_ntop konvertiert die binäre Repräsentation der Adresse in lesbare Repräsentation
		*/
		char *inet_ntop_retVal = inet_ntop(AF_INET, &sa_client, clientAddress, INET_ADDRSTRLEN); 
		inet_ntop_retVal == NULL ? perror("Fehler beim Konvertieren der Adresse") : printf("Client verbunden mit dieser Adresse: %s\n", clientAddress); 
		

		if (recv(sfd_communication, info, sizeof(info), 0))
		{
			printf("Message received: %s \n", info);
			if(info == NULL)
			{
				printf("Error: Message ist null!\n");
				return 1; 
			}

			//Hier werden auf die eingehenden Nachrichten reagiert --> Handle-Methoden
			if(strncmp("Get", info, 4) == 0)
			{
				handleGet(info, sfd_communication); 
			}else
			if(strncmp("Put", info, 4) == 0)
			{
				handlePut(info, sfd_communication, sizeof(info)); 
			}else
			if(strncmp("Files", info, 5) == 0)
			{
				handleFiles(info); 
			}else
			if(strncmp("List", info, 4) == 0)
			{
				handleList(info); 
			}else
			if(strncmp("Quit", info, 5) == 0)
			{
				handleQuit(info); 
			}else
			{
				//ungültiges Kommando vom Client
				//TODO darauf entsprechend reagieren 
				printf("Ungütliges Kommando vom Client\n"); 
			}
		}
	}
	close(sfd_listener);
	//TODO: Muss freeaddrinfo() vor oder nach close aufgerufen werden 
	freeaddrinfo(serverInfo);
	return 0; 
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
	//send wird nur aufgerufen wenn eine Verbindung zum Client besteht. 
	char* filename = strtok(arg + 4, " "); 

	//TODO hier muss mit send() gearbeitet werden um Nachrichten an den Client Socket zu schicken. 
	FILE *file = fopen(filename, "r"); 
	if(file == NULL)
	{
		perror("Fehler beim Öffnen der Datei"); 
	}

	int fseekRetVal; 
	if((fseekRetVal =  fseek(file, 0, SEEK_END)) != 0)
	{perror
		perro("Error in fseek"); 
	} 
	long fileSize = ftell(file);  
	if((fseekRetVal =  fseek(file, 0, SEEK_SET)) != 0)
	{
		perro("Error in fseek"); 
	} 

	char* fileData = (char*)malloc(fileSize); 
	if(fileData == NULL)
	{
		perror("Fehler bei der Speicherzuweisung"); 
		if(fclose(file) != 0)
		{
			perro("Error in flclose"); 
		}
	}

	size_t bytesRead = fread(fileData, sizeof(char), fileSize, file); 
	if(bytesRead != fileSize)
	{
		perror("Fehler beim Lesen der Datei"); 
		if(fclose(file) != 0)
		{
			perro("Error in flclose"); 
		} 
		free(fileData); 
	}
	if(fclose(file) != 0)
	{
		perro("Error in flclose"); 
	} 
	// Aktuelle Uhrzeit ermitteln
    time_t currentTime;
    if((currentTime = time(&currentTime)) == currentTime - 1)
	{
		perror("Error in time"); 
	}
    
	struct tm* timeInfo = localtime(&currentTime);
    char formattedTime[64];
    int retVal; 
	retVal =  strftime(formattedTime, sizeof(formattedTime), "%d-%m-%Y %H:%M:%S", timeInfo);
	if(retVal == 0)
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
	int retVal = sprintf(response, "Datei: %s\nDateigröße: %ld\nLetzte Änderung: %s\n\n%s", filename, fileSize, formattedTime, fileData); 

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

	// TODO: Empfange die Datei vom Client und speichere sie
	FILE *file = fopen(filename, "w"); 
	if(file == NULL)
	{
		perror("Fehler beim Öffnen der Datei");  
	}

	//Dateiinhalt parsen
	char dateiInhalt[253]; //Größe ist 253 da 256 Byte - 4 Byte = 252 Byte und +1 Byte wegen \0. 
	strncpy(dateiInhalt, arg + 4, 253); //arg + 4, damit der Pointer 4 Byte nach rechts geht da die ersten 3 Byte der Befehl Put sind und dann Leerzeichen. 

	size_t bytesWritten = fwrite(dateiInhalt, sizeof(char), dataLength-3, file); 
	if (bytesWritten != dataLength)
    {
        perror("Fehler beim Schreiben der Daten in die Datei");
        fclose(file);
    }
	fclose(file); 
	printf("Datei lokal auf den Server gespeichert.\n"); 

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
 * @return  
*/
void* handleList(char *arg)
{

	return NULL;
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
void* handleFiles(char *arg)
{

	return NULL;
}

/**
 * @brief Diese Funktion handled die Quit Anfragen der Clienten.
 * Bei einem Quit wird die Verbindung zwischen Server und Client aus der Clientseite beendet
 * 
 * @return  
*/
void* handleQuit(char *arg)
{
	return NULL; 
}