#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <netdb.h>

#define BUFFER_SIZE 1024

#define EOT 0x04


int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <server_address> <server_port>\n", argv[0]);
        exit(1);
    }

    const char *server_address = argv[1];
    const char *server_port = argv[2];

    struct addrinfo hints, *server_info;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC; // IPv4 und IPv6
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = 0;
    hints.ai_protocol = 0;

    int ret = getaddrinfo(server_address, server_port, &hints, &server_info);
    printf("Server Adresse: %s\n", server_address);
    printf("Server Port: %s\n", server_port);
    printf("ret: %d\n", ret);
    if (ret != 0) {
        fprintf(stderr, "getaddrinfo() failed: %s\n", gai_strerror(ret));
        exit(1);
    }

    int client_socket = socket(server_info->ai_family, server_info->ai_socktype, server_info->ai_protocol);
    printf("socket: %d\n", client_socket);
    if (client_socket == -1) {
        perror("socket");
        exit(1);
    }

    if (connect(client_socket, server_info->ai_addr, server_info->ai_addrlen) == -1) {
        //printf("Server Info Addr: %d",inet_ntoa(server_info->ai_addr));
        printf("Ich bin im Error drin\n");
        perror("connect");
        close(client_socket);
        exit(1);
    }
    printf("Zeile 48 guuuuut\n");

    freeaddrinfo(server_info);

    char command[BUFFER_SIZE];
    memset(&command, 0, sizeof(command));


    while (1) {
        printf("Enter a command (List, Files, Get <filename>, Put <filename>, or Quit): ");
        
        if (fgets(command, sizeof(command), stdin)== NULL){
            perror("fgets (command) ");
            exit(1);
        }

        command[strcspn(command, "\n")] = '\0';

        if (strncmp(command, "Quit", 4) == 0) {
            break;
        }

        

        // --------------------- Put Eingabe

        if (strncmp(command, "Put", 3) == 0) {
            char filename[32];
            memset(&filename, 0, sizeof(filename));

            sscanf(command, "Put %s", filename);
            // "Put" an den Server schicken
            if (send(client_socket, command, strlen(command), 0) == -1) {
                perror("send");
                continue;
            }
            //Ack vom server
            int n = recv(client_socket, command, sizeof(command), 0);
            if (n <= 0) {
                perror("recv");
                continue;
            }

            //Prüfen ob der Datei acknowledged wurde
            if (strncmp(command, "Filename received", 17) != 0) {
                fprintf(stderr, "Server did not acknowledge the filename\n");
                continue;
            }

            char filepath[BUFFER_SIZE];
            snprintf(filepath, sizeof(filepath), "../../src/data/%s", filename);
            FILE *file = fopen(filepath, "rb");
            if (file == NULL) {
                fprintf(stderr, "Failed to open file: %s\n", filename);
                continue;
            }

            // Datei an den Server schicken
            int bytes_read;
            while ((bytes_read = fread(&command, 1, sizeof(command), file)) > 0) {

                send(client_socket, command, bytes_read, 0);
                memset(&command, 0, sizeof(command));
                
            }
            // Das End of Transmission mitschicken
            command[0] = EOT;
            send(client_socket, command, 1, 0);

            fclose(file);
            printf("File sent: %s\n", filename);
            memset(&filename, 0, sizeof(filename));

        } else {
            
            send(client_socket, command, strlen(command), 0);
        }

       

        // -------------Get Eingabe
        
        if (strncmp(command, "Get", 3) == 0) {
            char buffer[BUFFER_SIZE];
            memset(&buffer, 0, sizeof(buffer));
            int bytes_received;

            while (1) {
                bytes_received = recv(client_socket, buffer, sizeof(buffer)-1, 0);
                
                if (bytes_received <= 0) {
                    printf("Error or Connection Closed\n");
                    break;
                }

                buffer[bytes_received] = '\0'; 

                // prüfen ob wir am Ende der Datei angekommen sind
                if (buffer[bytes_received-1]==EOT) {
                    printf("%s\n",buffer);
                    fflush(stdout);
                    memset(buffer, 0, sizeof(buffer)); 
                    break;
                }

              
                
                //buffer[bytes_received] = '\0'; 
                printf("%s\n",buffer);
                fflush(stdout);
                memset(buffer, 0, sizeof(buffer));
                }
        }

        // -------------Files Eingabe
        
        else if (strncmp(command, "Files", 5) == 0) {
            char buffer[BUFFER_SIZE];
            memset(&buffer, 0, sizeof(buffer));
            int bytes_received;

            while (1) {
                bytes_received = recv(client_socket, buffer, sizeof(buffer)-1, 0);
                
                if (bytes_received <= 0) {
                    printf("Error or Connection Closed\n");
                    break;
                }

                buffer[bytes_received] = '\0'; 

                // prüfen ob wir am Ende der Datei angekommen sind
                if (buffer[bytes_received-1]==EOT) {
                    printf("Response: %s\n",buffer);
                    fflush(stdout);
                    memset(buffer, 0, sizeof(buffer)); 
                    break;
                }
     
                //buffer[bytes_received] = '\0'; 
                printf("%s\n",buffer);
                fflush(stdout);
                memset(buffer, 0, sizeof(buffer));
                }
        }
        else{

            int n = recv(client_socket, command, sizeof(command), 0);
            if (n > 0) {
                printf("Response: %.*s\n", n, command);
            }
        }
    }

    //close(client_socket);
    return 0;
}
