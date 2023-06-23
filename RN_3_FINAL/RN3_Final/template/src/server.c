#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>  // a number of definition of data types used in system calls
#include <sys/socket.h>
#include <netdb.h>
#include <poll.h>
#include <time.h>
#include <sys/stat.h>
#include <dirent.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#define SIZE 1024
#define MAX_VERBINDUNG 10
#define SIZE_FILE_RESPONSE 10000
#define RECEIVE_BUFFER 256
#define FILE_SIZE 100

char resolvedHost[NI_MAXHOST];
char hostname[NI_MAXHOST];
char host_IP[INET6_ADDRSTRLEN];

struct CLIENT{
	char hostname[NI_MAXHOST];
	char port[NI_MAXSERV];
	int sd;
};
bool operator<(const CLIENT& l_1, const CLIENT& l_2)
{
  return l_1.sd < l_2.sd;
}

std::set<int> socketSet;
std::set<struct CLIENT> clientSet;




/**
 *  Handle Quit request. Close the Connection for sd
 *  @param sd : socket descriptor
 **/
void performQuit(int sd){
	std::cout << "Closing SD "<<sd<< std::endl;
	close(sd);
}

/**
 *  Handle Get request
 *  @param sd : socket descriptor
 *  @param buffer: request (Get + filename)
 **/
void performGet(int sd, const char* buffer){
	char str[10]={0};
	char filename[50]={0};
	char dataSize[FILE_SIZE]={0};
	char last_modified_time[100]={0};
	char header[1024]={0};
	struct stat attrib;
	stat(filename, &attrib);
	clock_t t1, t2;  
	sscanf(buffer,"%s %s",str,filename);
	FILE * fp = NULL;
	if(!(fp = fopen(filename,"r"))){
		snprintf(dataSize,sizeof(dataSize),"-1");
		if(send(sd,(char*)dataSize,sizeof(dataSize),0)>0){
			return ;
		}
		std::cout<<"Failed to send on SD "<<sd<<std::endl;
	}else{
		// Get data size
		fseek(fp,0L,SEEK_END);
		int size = ftell(fp);
		rewind(fp); 
		// Send data size
		bzero(dataSize,FILE_SIZE);
		snprintf(dataSize,sizeof(dataSize),"%d",size);
		printf("data size %d\n",size);
		if(send(sd,(char*)&dataSize,sizeof(dataSize),0)<0){
			perror("data size send error\n");
			return;
		}

		// Send header
			// Header
		strftime(last_modified_time, 100, "%Y-%m-%d %H:%M:%S", localtime(&attrib.st_mtime));
		snprintf(header,sizeof(header),"File size: %d\nLast modified : %s\n",size,last_modified_time);
		
		// Send Header
		if(send(sd,(char*)&header,sizeof(header),MSG_NOSIGNAL|MSG_DONTWAIT)<0){
			perror("Header send error\n");
			return;
		}


		
		// Send File content
		t1 = clock();
		char * buf = (char*) malloc(SIZE+1);
		if(!buf){
			perror("malloc failed()\n");
			return;
		}
		ssize_t received = 0;
		int sent = 0;
		while(sent < size){
			bzero(buf,SIZE+1);
			received = fread(buf,1,SIZE,fp);		
			if(received <0){
				perror("read() call failed \n");
				return;
			}
			buf[received] = '\x04';
			if(send(sd,(char*)buf,SIZE,0)<0){
				perror("file send error\n");
				return;
			}
			sent+=received;
		}
		t2 = clock();
		printf("sent: %d bytes\n",sent);
		free(buf);  
		fclose(fp);

		// Transmission Time
		float diff = ((float)(t2 - t1) / CLOCKS_PER_SEC ) * 1000;
		bzero(last_modified_time,100);
		snprintf(last_modified_time,sizeof(last_modified_time),"\nTransmission Time: %f ms\n",diff);
		
		if(send(sd,(char*)last_modified_time,sizeof(last_modified_time),0)<0){
			perror("Transmissin time: send error\n");
			return;
		}
	}
}


/**
 *  Handle Put request
 *  @param sockfd : socket descriptor
 *  @param buffer: request (Put + filename)
 *  @param hostname: server name
 *  @param ip_Addresse: IP Adresse of server
 **/
bool performPut(int sockfd,const char *buffer,const char *hostname,const char *ip_Addresse){
	FILE *fp;
	
	char *data = NULL;
	char str[10];
	char filename[50];
	char response[SIZE];
	sscanf(buffer,"%s %s",str,filename);
	
	fp = fopen(filename, "w");
	if (fp == NULL){
		perror("fopen() failed\n");
		return true;
	}
	
	time_t t = time(NULL);
	struct tm tm = *localtime(&t);

	//Read file size
	char f_size[FILE_SIZE]={0};
	ssize_t received = recv(sockfd,(char*)f_size,sizeof(f_size),MSG_WAITALL);
	if(received <0){
		perror("size recv() call failed \n");
		return  true;
	}
	else if (received == 0){
		// Connection closed without error
		perror("Client disonnected\n");
		performQuit(sockfd);
		return false;
	}

	int dataSize = atoi(f_size);
	data = (char*) malloc(SIZE+1);
	if(data == NULL){
		perror("malloc() failed\n");
		return true;
	}
	int arrived = 0;
	while(arrived < dataSize){
		bzero(data, SIZE+1);
		printf("waiting rest data...\n");
		received = recv(sockfd,data,sizeof(data), MSG_WAITALL);
		printf("received %ld \n", received);
		//printf("received...\n");
		if(received <0 ){
			perror("data recv() call failed \n");
			fclose(fp);
			return true;
		}
		else if (received == 0){
			// Connection closed without error
			fclose(fp);
			perror("Client disonnected\n");
			performQuit(sockfd);
			return false;
		}
		

		for(int i = 0; i < 8; i++){
			char c = data[i];
			if(c != '\x04' && c != '\0'){
				fputc(c, fp);
			} else {
				break;
			}
		}
		
		arrived += received;
	}
	

	fseek(fp, 0, SEEK_END);
	int size = ftell(fp);
	rewind(fp);
	fclose(fp);
	printf("received %d bytes, datasize: %d bytes\n",size,dataSize);
	if(size != (dataSize))
	{
		perror("Error saving file\n");
		return true;
	}

  	free(data);
	//serverhostname/benutzte ip Addresse und Datum/uhrzeit zurück an client schicken 
	bzero(response, SIZE);
	snprintf(response,SIZE,"Server = %s\nServer IP = %s\nCurrent Date and time = : %d-%02d-%02d %02d:%02d:%02d\n"
	   ,hostname,ip_Addresse,tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour+2, tm.tm_min, tm.tm_sec);
	
	if(send(sockfd,response,sizeof(response),0)<0){
		perror("error beim senden");
		return true;
	}
	printf("File saved successfully\n");
	return true;
}




/**
 *  Handle Files request. 
 *  @param sd : socket descriptor
 **/
void performFiles(int sd)
{
	char data[SIZE_FILE_RESPONSE]={0};
	char teildata[1000]={0};
	struct stat filestat;
	DIR *dir = opendir(".");
	stat("gettysburg.txt", &filestat);
	if (dir == NULL)
	{
		return;
	}

	struct dirent *entity;
	entity = readdir(dir);
	while (entity != NULL)
	{   
		stat(entity->d_name, &filestat);
		if (strcmp(entity->d_name, ".") != 0)
		{
			if (strcmp(entity->d_name, "..") != 0)
			{
				bzero(teildata,sizeof(teildata));
				sprintf(teildata,"Filename: %s Last modified: %s size: %lu bytes\n",entity->d_name,ctime(&filestat.st_mtime),filestat.st_size);
				strcat(data, teildata);
			}
		}

		entity = readdir(dir);
	}
	send(sd, data, sizeof(data), 0);

	closedir(dir);	
}



/**
 *  Handle List request.
 *  @param sd : socket descriptor
 **/
void performList(int sd){
	char buffer[NI_MAXHOST+NI_MAXSERV+3];
	for(auto client : clientSet){
		
		bzero(buffer,sizeof(buffer));
		sprintf(buffer,"%s:  %s\n",client.hostname,client.port);
		if(send(sd,(char*)&buffer,sizeof(buffer),0)<0){	
			perror("Error send\n");
			return ;
		}
	}
	bzero(buffer,sizeof(buffer));
	sprintf(buffer,"end");
	if(send(sd,buffer,sizeof(buffer),0)<0){	
		perror("Error send\n");
		return ;
	}
}

/**
 * Handle request from a socket descriptor 
 **/
bool performApplication(int sd){
	char buffer[RECEIVE_BUFFER]={0};
	ssize_t bytesRead;
	if((bytesRead = recv(sd,(char*)&buffer, sizeof(buffer),MSG_WAITALL)) > 0){
		if(!strncmp(buffer,"Get ",4)){
			performGet(sd,buffer);
		}else if(strncmp(buffer,"Quit",4) == 0){
			performQuit(sd);
			return false;
		}else if(strncmp(buffer,"List",4) == 0){
			performList(sd);
		}else if(strncmp(buffer,"Put ",4) == 0){
			//printf("put wird ausgefuhrt\n");
			return performPut(sd,buffer,hostname,host_IP);
		}else if(strncmp(buffer,"Files",5) == 0){
			performFiles(sd);
		}	
		
    }
	
    return true;
}

int main(int argc, char** argv){

	if(argc < 2){
		//std::cerr <<"Usage: "<<argv[0]<<"[Port]"<<std::endl;
		//exit(1);
		argv[1] =(char*) "0";
	}
	const char* port = argv[1];
	gethostname(hostname,sizeof(hostname));

	//======= Get remote addres (resolve hostname and service)===========
	struct addrinfo * ainfo = NULL;
	struct addrinfo ainfohint;
	memset((char*)&ainfohint,0,sizeof(ainfohint));
	
	ainfohint.ai_flags = AI_PASSIVE; // AI_PASSIVE will set address to the ANY address
	ainfohint.ai_family = PF_INET6;
	ainfohint.ai_socktype = SOCK_STREAM;
	ainfohint.ai_protocol = IPPROTO_TCP;
    
	int error = getaddrinfo(NULL,port,&ainfohint,&ainfo);
	if (error!=0){
		std::cerr<<"ERROR: getaddrinfo() failed"<< gai_strerror(error)<<std::endl;
		exit(1); 
	}
    // ===================== Create Socket =================================
    int sd = socket(ainfo->ai_family,ainfo->ai_socktype,ainfo->ai_protocol);
    if(sd<=0){
    	perror("Unable to create socket");
     	exit(1);
     }
     // =================== Bind to local port ===========================
     if(bind(sd,ainfo->ai_addr, ainfo->ai_addrlen)<0){
     	perror("bind() call failed");
     	exit(1);
    }

	// =================== Get used IP-Adress =================================
	
	struct sockaddr_in6 *serverIP = (struct sockaddr_in6*)ainfo->ai_addr;
	struct in6_addr ip_addr = serverIP->sin6_addr;     /* IPv6 address */
 	inet_ntop(AF_INET6,&ip_addr,host_IP,INET6_ADDRSTRLEN);

	printf("Server running\n");


     if(listen(sd,MAX_VERBINDUNG)<0){
     	perror("listen() call failed");
		exit(1);
     }

     // =========================== Main loop =============================
     for(;;){
		//============= Wait for events =================
     	const size_t connections = socketSet.size();
     	pollfd pfd[1+connections];
     	pfd[0].fd = sd;
     	pfd[0].events = POLLIN;// POLLIN Flag bedeutet dass wir auf Eingang von Daten warten

     	int i = 1;
     	for(auto iterator = socketSet.begin(); iterator != socketSet.end(); iterator++){
     		pfd[i].fd = *iterator;
     		pfd[i].events = POLLIN;
     		i++;
     	}

     	int events = poll((pollfd*)&pfd,1 + connections, -1);
     	// ========== handle events ==========================
		
     	if (events > 0)
     	{
			
     		// ========== Accept incoming connection =============
           
     		if (pfd[0].revents & POLLIN)
     		{  

     			// ============ Accept incoming connection
		     	sockaddr_storage remoteAddress;
		     	socklen_t remoteAddressLength = sizeof(remoteAddress);
		     	int newSD = accept(sd,(sockaddr*)&remoteAddress, &remoteAddressLength);
		     	if(newSD < 0){
		     		perror("accept() call failed");
		     		break;
		     	}
                struct CLIENT client;
				error = getnameinfo((struct sockaddr*)&remoteAddress,remoteAddressLength,(char*)&client.hostname,sizeof(client.hostname),(char*)&client.port,sizeof(client.port),0);
				if(error != 0){
					std::cerr <<"ERROR: getnameinfo() failed: "<<gai_strerror(error)<<std::endl;
					//exit(1);
				}
				//printf("Host: %s Port: %s\n",client.hostname,client.port);
				client.sd = newSD;
				socketSet.insert(newSD);
				clientSet.insert(client);	
     		}
			
     		// =========== Handle incoming data on a connection ==============
     		for(size_t i = 1; i<= connections;++i){
               
     			if(pfd[i].revents & POLLIN){
     				// ==== Perform application ==========
     				if(performApplication(pfd[i].fd) == false){
     					socketSet.erase(pfd[i].fd); // Connection has been closed!
						for (auto it = clientSet.begin(); it!=clientSet.end();++it){
							if(it->sd == pfd[i].fd){
								//printf("sd = %d\n",it->sd);
								clientSet.erase(it);
								break;
								
							}
						}
   							 
     				}
     			}
     		}
     	}
     	
    }
     	
     	    

     // ============================ Clean up =================================
    close(sd);
	freeaddrinfo(ainfo);

	return 0;
}