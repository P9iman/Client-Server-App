/**
 * @brief Diese Funktion handled die List Anfragen der Clienten. 
 * Beim list werden alle verbundenen Clients der Form: 
 * <Clienthostname>:<Clientport>
 * <N> Clients verbunden
 * ausgegeben. 
 * 
 * @return  
*/
void* handleList(char *arg, int socketFd)
{
    char response[BUFFER_SIZE]; // Puffer fuer die Antwortnachricht
    char client_list[BUFFER_SIZE]; // Puffer fuer die Clientliste
    sprintf(client_list, "List:\n"); // "List:\n" in die Clientliste schreiben

    // ueber verbundene Clients iterieren
    for (int i = 0; i < num_clients; i++)
	{
        char client_info[BUFFER_SIZE]; // Puffer fuer Informationen ueber einen Client
        sprintf(client_info, "%s:%d\n", clients[i].hostname, clients[i].port); // Informationen ueber den aktuellen Client formatieren
        strcat(client_list, client_info); // Informationen des Clients zur Clientliste hinzufuegen
    }
    
    sprintf(response, "List:\n%s<N> Clients verbunden\n", client_list); // Endgueltige Antwortnachricht generieren, <N> wird durch die Anzahl der verbundenen Clients ersetzt

    
    send(socketFd, response, strlen(response), 0); // Antwortnachricht an den Client senden

    return NULL; // Funktion beenden und NULL zurueckgeben
}

/**
 * @brief Diese Funktion handled die Files Anfragen der Clienten. 
 * Bei Files wird eine Liste aller Dateien im Server-Verzeichnis in der Form: 
 * <Dateiname> <Datei-Attribute: last modified, size>
 * <N> Dateien
 * ausgegeben. 
 * @param 
 * 
 * @return
*/
void* handleFiles(char* arg, int socketFd)
{
    // Vorbereiten des Antwortstrings
    char response[BUFFER_SIZE];
    char file_list[BUFFER_SIZE];
    sprintf(file_list, "Files:\n");

    // Oeffnen des Serververzeichnisses
    DIR* dir = opendir(".");
    if (dir == NULL)
	{
        perror("Fehler beim Öffnen des Verzeichnisses");
        return NULL;
    }

    struct dirent* entry;
    int file_count = 0;

    // Verzeichniseintraege lesen
    while ((entry = readdir(dir)) != NULL)
	{
        // Verzeichnisse und versteckte Dateien ueberspringen
        if (entry->d_type == DT_DIR || entry->d_name[0] == '.')
		{
            continue;
        }

        // Dateiinformationen abrufen
        struct stat file_stat;
        char file_info[BUFFER_SIZE];

        if (stat(entry->d_name, &file_stat) == -1)
		{
            perror("Fehler beim Abrufen der Dateiinformationen");
            closedir(dir);
            return NULL;
        }

        // Dateiinformationen formatieren
        sprintf(file_info, "%s Last Modified: %s Size: %ld\n", entry->d_name,
                ctime(&file_stat.st_mtime), file_stat.st_size);
        strcat(file_list, file_info);

        file_count++;
    }

    // Verzeichnis schliessen
    closedir(dir);

    // Endgueltige Antwort generieren
    sprintf(response, "Files:\n%s%d Dateien\n", file_list, file_count);

    // Antwort an den Client senden
    send(socketFd, response, strlen(response), 0);
    
	return NULL;
}


/**
 * @brief Diese Funktion handled die Quit Anfragen der Clienten.
 * Bei einem Quit wird die Verbindung zwischen Server und Client aus der Clientseite beendet
 * 
 * @return  
*/
void* handleQuit(char *arg, int socketFd)
{
	// Erstellen der Abschlussnachricht
    char response[BUFFER_SIZE];
    sprintf(response, "Verbindung wird beendet.\n");

    // Antwort an den Client senden
    send(socketFd, response, strlen(response), 0);

    // Schliessen der Socket-Verbindung
    close(socketFd);

	num_clients--;

}