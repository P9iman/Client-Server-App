#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/select.h>
#include <netdb.h>
#include <dirent.h>
#include <sys/stat.h>
#include <time.h>

#define EOT 0x04
#define BACKLOG 5
#define BUFFER_SIZE 1024

#include <sys/sendfile.h>
#include <fcntl.h>


void handleGetCommand(int client_socket, const char* filename) {
    // // Create the file path by concatenating the folder name and the filename
    // char file_path[64];
    // memset(&file_path, 0, sizeof(file_path));

    // snprintf(file_path, sizeof(file_path), "../../src/files/%s", filename);

    // Datei öffnen
    char filepath[64];
    memset(&filepath, 0, sizeof(filepath));

    snprintf(filepath, sizeof(filepath), "../../src/files/%s", filename);
    FILE *file = fopen(filepath, "rb");
    if (file == NULL) {
        fprintf(stderr, "Failed to open file: %s\n", filename);
        return;
    }

    // Datei an den client schicken
    char buffer[1024];
    memset(&buffer, 0, sizeof(buffer));

    int bytes_read;

    while ((bytes_read = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        
        send(client_socket, buffer, bytes_read, 0);
        memset(buffer, 0, sizeof(buffer));
        
    }
    //Dateiende mitschicken
    buffer[0] = EOT;
    send(client_socket, buffer, 1, 0);


    fclose(file);
    memset(buffer, 0, sizeof(buffer)); 
    printf("File sent: %s\n", filename);

}




void handlePutCommand(int client_socket, const char* filename) {
    char filepath[1024];
    char buffer[1024];
    memset(&filepath, 0, sizeof(filepath));
    memset(&buffer, 0, sizeof(buffer));


    // Einen Pfad mit dem Dateinamen erstellen
    snprintf(filepath, sizeof(filepath), "../../src/files/%s", filename);

    FILE* file = fopen(filepath, "wb");
    if (file == NULL) {
        perror("fopen");
        return;
    }

    int bytes_received;
    while (1) {
        bytes_received = recv(client_socket, buffer, sizeof(buffer)-1, 0);
        
        if (bytes_received <= 0) {
            printf("Error or Connection Closed\n");
            break;
        }

        buffer[bytes_received] = '\0';

        //Prüfen ob wir am Ende der Datein angekommen sind und den Rest schicken
        if (buffer[bytes_received-1]==EOT) {
            buffer[bytes_received-1] = '\0';
            printf("----- %s \n",buffer);
            printf("End of File\n");
            fflush(stdout);
            int bytes_written = fwrite(buffer, 1, bytes_received-1, file);
            if (bytes_written != bytes_received-1) {
                printf("Error in Writing\n");
                perror("fwrite");
                fclose(file);
                remove(filepath); 
            }
            break;
        }

        //Den aktuellen Bufferinhalt in die Datei schreiben    
        //buffer[bytes_received] = '\0'; 
        int bytes_written = fwrite(buffer, 1, bytes_received, file);
        if (bytes_written != bytes_received) {
            printf("Error in Writing\n");
            perror("fwrite");
            fclose(file);
            remove(filepath); 
            break;
        }
            
            memset(buffer, 0, sizeof(buffer)); // Clear the buffer
        }
    
    fclose(file);
    
    printf("File saved: %s\n", filepath);
    send(client_socket,"File received", 13, 0); //Ack an den Clienten schicken
}



void handleFilesCommand(int client_socket) {

    const char* folder_path = "../../src/files";
    DIR* folder = opendir(folder_path);
    if (folder == NULL) {
        perror("opendir");
        return;
    }
    struct dirent* entry;
    int total_files = 0;
    // Durch die Liste der Dateien iterieren
    while ((entry = readdir(folder)) != NULL) {
        if (entry->d_type == DT_REG){

            char file_path[BUFFER_SIZE];
            memset(&file_path, 0, sizeof(file_path));
            snprintf(file_path, sizeof(file_path), "%s/%s", folder_path, entry->d_name);
            struct stat file_stat;
            if (stat(file_path, &file_stat) == -1) {
                perror("stat");
                continue;
            }

            // Information der Datei Formats erlangen
            char file_info[BUFFER_SIZE];
            memset(&file_info, 0, sizeof(file_info));
            const char* file_name = entry->d_name;
            time_t modified_time = file_stat.st_mtime;
            off_t file_size = file_stat.st_size;
            snprintf(file_info, sizeof(file_info), "%s %s %ld\n", file_name, ctime(&modified_time), file_size);

            // Datei informationen an den Clienten schicken
            send(client_socket, file_info, strlen(file_info), 0);
            total_files++;
        }
    }


    closedir(folder);

   
    char response[BUFFER_SIZE];
    memset(&response, 0, sizeof(response));
    snprintf(response, sizeof(response), "Es gibt %d Dateien\n", total_files);  // Die Anzahl der Dateinen schicken
    send(client_socket, response, strlen(response), 0);
    memset(&response, 0, sizeof(response));
    //Dateiende mitschicken
    response[0] = EOT;
    send(client_socket, response, 1, 0);
}

void handleListCommand(int client_socket, int max_fd, fd_set* master_set) {
    // Durch die Liste der Verbundenen Clienten iterieren
    char client_info[2048];
         memset(&client_info, 0, sizeof(client_info));

    int total_clients = 1;

    for (int fd = 0; fd <= max_fd; fd++) {
        if (FD_ISSET(fd, master_set)) {
            if (fd != client_socket && fd != STDIN_FILENO && fd != STDOUT_FILENO) {
                struct sockaddr_storage client_addr;
                socklen_t addr_len = sizeof(client_addr);
                if (getpeername(fd, (struct sockaddr*)&client_addr, &addr_len) == 0) {
                    char client_host[NI_MAXHOST];
                    char client_port[NI_MAXSERV];
                    if (getnameinfo((struct sockaddr*)&client_addr, addr_len,
                                    client_host, sizeof(client_host),
                                    client_port, sizeof(client_port),
                                    NI_NUMERICHOST | NI_NUMERICSERV) == 0) {
                        //IP und Port 
                        sprintf(client_info, "IP %s: Port %s\n", client_host, client_port);
                        send(client_socket, client_info, strlen(client_info), 0);
                        total_clients++;
                    }
                }
            }
        }
    }

   //Die Anzahl der verbundenen Clienten schicken
    sprintf(client_info, "%d Clients verbunden\n", total_clients);
    send(client_socket, client_info, strlen(client_info), 0);
}


int main(int argc, char* argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        exit(1);
    }

    const char* port = argv[1];

    struct addrinfo hints, *server_info;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;     // IPv4 and IPv6
    hints.ai_socktype = SOCK_STREAM; // TCP socket
    hints.ai_flags = 0;
    hints.ai_protocol = 0;

    int ret = getaddrinfo(NULL, port, &hints, &server_info);
    if (ret != 0) {
        fprintf(stderr, "getaddrinfo() failed: %s\n", gai_strerror(ret));
        exit(1);
    }

    int server_socket = socket(server_info->ai_family, server_info->ai_socktype, server_info->ai_protocol);
    if (server_socket == -1) {
        perror("socket");
        exit(1);
    }

    int optval = 1;
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) == -1) {
        perror("setsockopt");
        exit(1);
    }

    if (bind(server_socket, server_info->ai_addr, server_info->ai_addrlen) == -1) {
        perror("bind");
        exit(1);
    }

    if (listen(server_socket, BACKLOG) == -1) {
        perror("listen");
        close(server_socket);
        exit(1);
    }

    // Obtain the actual port number assigned by the OS
    struct sockaddr_storage server_addr;
    socklen_t server_addr_len = sizeof(server_addr);
    if (getsockname(server_socket, (struct sockaddr*)&server_addr, &server_addr_len) == -1) {
        perror("getsockname");
        exit(1);
    }

    char server_addr_str[NI_MAXHOST];
    char server_port_str[NI_MAXSERV];
    ret = getnameinfo((struct sockaddr*)&server_addr, server_addr_len,
                      server_addr_str, sizeof(server_addr_str),
                      server_port_str, sizeof(server_port_str),
                      NI_NUMERICHOST | NI_NUMERICSERV);
    if (ret != 0) {
        fprintf(stderr, "getnameinfo() failed: %s\n", gai_strerror(ret));
        exit(1);
    }

    printf("Server listening on %s:%s\n", server_addr_str, server_port_str);

    fd_set master_set, read_set;
    int max_fd;

    FD_ZERO(&master_set);
    FD_SET(server_socket, &master_set);
    max_fd = server_socket;

    while (1) {
        read_set = master_set;
        if (select(max_fd + 1, &read_set, NULL, NULL, NULL) == -1) {
            perror("select");
            exit(1);
        }

        for (int fd = 0; fd <= max_fd; fd++) {
            if (FD_ISSET(fd, &read_set)) {
                if (fd == server_socket) {
                    // Neue Verbindungsanfrage
                    struct sockaddr_storage client_addr;
                    socklen_t client_addr_len = sizeof(client_addr);
                    int client_socket = accept(server_socket, (struct sockaddr*)&client_addr, &client_addr_len);
                    if (client_socket == -1) {
                        perror("accept");
                        exit(1);
                    }

                    FD_SET(client_socket, &master_set);
                    if (client_socket > max_fd) {
                        max_fd = client_socket;
                    }

                    // Clienten information erhalten
                    char client_host[NI_MAXHOST];
                    char client_port[NI_MAXSERV];
                    ret = getnameinfo((struct sockaddr*)&client_addr, client_addr_len,
                                      client_host, sizeof(client_host),
                                      client_port, sizeof(client_port),
                                      NI_NUMERICHOST | NI_NUMERICSERV);
                    if (ret != 0) {
                        fprintf(stderr, "getnameinfo() failed: %s\n", gai_strerror(ret));
                        exit(1);
                    }

                    printf("New connection established. Client socket: %d\n", client_socket);
                } else {
                    // Existing connection has data to read
                    char buffer[BUFFER_SIZE];
                    memset(&buffer, 0, sizeof(buffer));
                    int n = recv(fd, buffer, sizeof(buffer), 0);
                    if (n == -1) {
                        perror("recv");
                        exit(1);
                    } else if (n == 0) {
                        // Client hat die Verbindung geschlossen
                        printf("Connection closed by client. Socket: %d\n", fd);
                        close(fd);
                        FD_CLR(fd, &master_set);
                    } else {
                        // Daten vom Client erhalten
                        printf("Received data from client. Socket: %d, Data: %.*s\n", fd, n, buffer);

                        // Wenn der Client "List" schickt                    
                        if (strncmp(buffer, "List", 4) == 0) {
                            handleListCommand(fd, max_fd, &master_set);
                        } else if (strncmp(buffer, "Quit", 4) == 0){
                            // Client hat die Verbindung geschlossen
                            printf("Connection closed by client. Socket: %d\n", fd);
                            close(fd);
                            FD_CLR(fd, &master_set);
                        } else if (strncmp(buffer, "Files", 5) == 0) {    
                            // handleFilesCommand(fd, max_fd, &master_set);
                            handleFilesCommand(fd);
                        } else if (strncmp(buffer, "Get", 3) == 0) {
                            
                            char* filename = buffer + 4;  // Ersten drei Zeichen überspringen

                            // "\n" aus den Daten entfernen
                            size_t filename_len = strlen(filename);
                            if (filename[filename_len - 1] == '\n') {
                                filename[filename_len - 1] = '\0';
                            }

                            handleGetCommand(fd, filename);
                        } else if (strncmp(buffer, "Put", 3) == 0) {
                            
                            char* filename = buffer + 4; // Ersten drei Zeichen überspringen/ Put überspringen
                            printf("Received filename: %s\n", filename);

                            //Ack an den clienten schicken
                            send(fd, "Filename received", 17, 0);

                            handlePutCommand(fd, filename);
                        } else {
                            // Ungültige Eingabe
                            const char* response = "Invalid command";
                            send(fd, response, strlen(response), 0);
                        }
                    }
                }
            }
        }
    }

    //close(server_socket);
    return 0;
}
