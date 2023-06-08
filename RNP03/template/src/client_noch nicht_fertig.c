
/**
 * @brief Sendet eine Get-Anfrage an den Server, um eine bestimmte Datei herunterzuladen.
 * 
 * @param socketFd Der Socket-Dateideskriptor zur Verbindung mit dem Server.
 * @param filename Der Name der herunterzuladenden Datei.
 */
void sendGetRequest(int socketFd, char* filename)
{
  char get_message[BUFFER_SIZE];
  sprintf(get_message, "GET %s", filename);
  send(socketFd, get_message, strlen(get_message), 0);
}

/**
 * @brief Sendet eine Put-Anfrage an den Server, um eine Datei auf den Server hochzuladen.
 * 
 * @param socketFd Der Socket-Dateideskriptor zur Verbindung mit dem Server.
 * @param filename Der Name der hochzuladenden Datei.
 */
void sendPutRequest(int socketFd, char* filename)
{
  char put_message[BUFFER_SIZE];
  sprintf(put_message, "PUT %s", filename);
  send(socketFd, put_message, strlen(put_message), 0);
}

/**
 * @brief Sendet eine Files-Anfrage an den Server, um eine Liste der verfuegbaren Dateien anzufordern.
 * 
 * @param socketFd Der Socket-Dateideskriptor zur Verbindung mit dem Server.
 */
void sendFilesRequest(int socketFd)
{
  char files_message[] = "FILES";
  send(socketFd, files_message, strlen(files_message), 0);
}


/**
 * @brief Sendet eine List-Anfrage an den Server, um eine Liste der verfuegbaren Dateien anzufordern.
 * 
 * @param socketFd Der Socket-Dateideskriptor zur Verbindung mit dem Server.
 */
void sendListRequest(int socketFd)
{
  char list_message[] = "LIST";
  send(socketFd, list_message, strlen(list_message), 0);
}

/**
 * @brief Sendet eine Quit-Anfrage an den Server, um die Verbindung zu beenden.
 * 
 * @param socketFd Der Socket-Dateideskriptor zur Verbindung mit dem Server.
 */
void sendQuitRequest(int socketFd)
{
  char quit_message[] = "QUIT";
  send(socketFd, quit_message, strlen(quit_message), 0);
}
