#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>  // a number of definition of data types used in system calls
#include <sys/socket.h>
#include <netdb.h>
#include <iostream>
#include <signal.h>
#define SIZE 1024
#define REQUEST_SIZE 256
#define FILE_SIZE 100
#define SIZE_FILE_RESPONSE 10000

int sd;
struct addrinfo* ainfo = NULL;

/**
 * Send Quit requests to the server
 * @param sd: socket descriptor
 **/
void quitRequest(int sd){
	char data[REQUEST_SIZE] = {0};
	snprintf((char*)&data, sizeof(data),"Quit");
    if(write(sd,data,sizeof(data))<0){
     	perror("write() call failed\n");
     	exit(1);
    }
	exit(EXIT_SUCCESS);
}

/**
 * Send Get request to the server
 * @param sd: socket descriptor
 * @param requestedFile: filename
 **/
void getRequest(int sd,const char * requestedFile){
	// ======== Request webpage==================
     //std::cout<<"Connected! Sending GET..."<<std::endl;
     char data[REQUEST_SIZE]={0};
     snprintf((char*)&data, sizeof(data),"Get %s \n",requestedFile);
	if(write(sd,data,sizeof(data))<0){
     	perror("write() call failed\n");
		quitRequest(sd);
     	exit(1);
    }


}

void send_file(FILE *fp, int sockfd,int fileSize){
    char *data = (char*) malloc(SIZE+1);
	ssize_t received = 0;
	int sent = 0;

	while(sent < fileSize){
		bzero(data,SIZE);
		received = fread(data,1,SIZE,fp);
		
		if(received <0){
			perror("read() call failed \n");
			quitRequest(sd);
			exit(1);
		}
		sent+=received;
		if(sent == fileSize){
			data[received] = '\x04';
			printf("4 \n");
		} else {
			data[received] = '\0';
			printf("0 \n");
		}
		
		if(send(sd,(char*)data,SIZE,0)<0){
			perror("send error\n");
			quitRequest(sd);
			exit(1);
		}
		
		
		//printf("data: %s\n",data);
	}
	printf("sent: %d bytes\n",sent);
    free(data);  
}

/**
 * Send Put request to the server, send file size then the data and wait for server reply
 * @param sd: socket descriptor
 * @param requestedFile: filename
 **/
void put(int sd,const char *filename)
{
	FILE *fp = NULL;
	char response[SIZE] = {0};
	char data1 [REQUEST_SIZE]={0};
	
	
	if(!(fp = fopen(filename,"r"))){
			perror("File not found\n");
			return;
	}
    snprintf((char*)&data1, sizeof(data1),"Put %s \n",filename);
	if(send(sd,data1,sizeof(data1),0)<0){
		perror("error beim senden");
		quitRequest(sd);
		exit(1);
	}
	
	fseek(fp,0L,SEEK_END);
	int size = ftell(fp);
	rewind(fp);

	char dataSize[FILE_SIZE]={0};	
	snprintf(dataSize,sizeof(dataSize),"%d",size);
	if(send(sd,(char*)&dataSize,sizeof(dataSize),0)<0){
		perror("send error\n");
		return;
	}

    send_file(fp,sd,size);
	if(recv(sd,response,sizeof(response),MSG_WAITALL)){
		printf("%s\n",response);
	}
    fclose(fp);
}

/* Signal Handler for SIGINT */
void sigintHandler(int sig_num)
{
    /* Reset handler to catch SIGINT next time.
       Refer http://en.cppreference.com/w/c/program/signal */
    signal(SIGINT, sigintHandler);
    printf("\n Connection Closed\n");
	quitRequest(sd);
	close(sd);
	freeaddrinfo(ainfo);
    fflush(stdout);
}

/**
 * Send Files request to the server
 * @param sd: socket descriptor
 **/
void filesRequest(int  sd){
	char data[REQUEST_SIZE]={0};
	snprintf((char*)&data, sizeof(data),"Files");
    if(write(sd,data,sizeof(data))<0){
     	perror("write() call failed\n");
		quitRequest(sd);
     	exit(1);
    }  //#include <netinet/in.h> // constants and structures needed for internet domain addresses
}

/**
 * Receive the ist of files after a Files request
 * @param sd: socket descriptor
 **/
void filesReceive(int sd){
	char data[SIZE_FILE_RESPONSE]={0};
	size_t received = recv(sd,(char*)data,sizeof(data),MSG_WAITALL);
	if(received != sizeof(data)){
		perror("read() call failed \n");
		quitRequest(sd);
		exit(1);
	}
	printf("%s",data);
}

/**
 * Send List request
 * @param sd: socket descriptor
 **/
void listRequest(int  sd){
	char data[REQUEST_SIZE]={0};
	snprintf((char*)&data, sizeof(data),"List");
    if(write(sd,data,sizeof(data))<0){
     	perror("write() call failed\n");
		quitRequest(sd);
     	exit(1);
    }
}

/**
 * Receive the file content after a Get request
 * @param sd: socket descriptor
 **/
void receiveFile(int sd){

	//Read file size
	char header[1024]={0};
	char datasize[FILE_SIZE]={0};

	ssize_t received = recv(sd,(char*)datasize,sizeof(datasize),MSG_WAITALL);
	if(received <0){
		perror("File size recv() call failed \n");
		quitRequest(sd);
		exit(1);
	}
	else if (received == 0){
		// Connection closed without error
		perror("Connection Closed\n");
		quitRequest(sd);
		exit(1);
	}


	int size = atoi(datasize);
	if(size == -1){
		printf("File not found\n");
		return;
	}
	// Receive header
	received = recv(sd,(char*)header,sizeof(header),MSG_WAITALL);
	if(received <0){
		perror("Header recv() call failed \n");
		quitRequest(sd);
		exit(1);
	}
	else if (received == 0){
		// Connection closed without error
		perror("Connection Closed\n");
		quitRequest(sd);
		exit(1);
	}
	printf("\n%s",header);
	//Receive file content


	char * content = (char*)malloc(SIZE+1);
	if(content == NULL){
		perror("malloc() failed \n");
		quitRequest(sd);
		exit(1);
	}
	int arrived = 0;
	while(arrived < size){
		bzero(content, SIZE+1);
		received = recv(sd,content,SIZE, MSG_WAITALL);
		if(received <0 ){
			perror("data recv() call failed \n");
			quitRequest(sd);
			exit(1);
		}
		else if (received == 0){
			perror("Server disonnected\n");
			exit(1);
		}
		content[received]='\0';
		printf("%s",content);
		arrived += received;
		//printf("arrived: %d",arrived);
		//printf("data: %s",data);
	}

	free(content);
	// Receive transmission Time
	char t_Time[100]={0};
	
	received = recv(sd,(char*)t_Time,100,MSG_WAITALL);
	if((int)received == 0){
		perror("Transm. time recv() call failed\n");
		quitRequest(sd);
		exit(1);
	}
	else if (received == 0){
		// Connection closed without error
		perror("Server disonnected\n");
		exit(1);
	}
	printf("%s",t_Time);
	
}

/**
 * Receive the file of connected Clients after List request
 * @param sd: socket descriptor
 **/
void receiveList(int sd){

	//Read file size
	char buffer[NI_MAXHOST+NI_MAXSERV+3];
	
	do{
		bzero(buffer,sizeof(buffer));
		ssize_t received = recv(sd,(char*)buffer,sizeof(buffer),MSG_WAITALL);
		if(received <0){
			perror("read() call failed \n");
			quitRequest(sd);
			exit(1);
		}
		else if (received == 0){
			// Connection closed without error
			perror("Connection Closed\n");
			quitRequest(sd);
			exit(1);
		}
		if(strncmp(buffer,"end",3)){
			printf("%s",buffer);
		}
	}while(strncmp(buffer,"end",3));
}

int main(int argc, char** argv){
	if(argc < 3){
		std::cerr <<"Usage: "<<argv[0]<<" [Remote Host] [Port]"<<std::endl;
		exit(1);
	}
	const char* remoteHost = argv[1];
	const char* port = argv[2];
	//======= Get remote addres (resolve hostname and service)===========
	
	struct addrinfo ainfohint;
	memset((char*)&ainfohint,0,sizeof(ainfohint));

	ainfohint.ai_family = PF_UNSPEC;
	ainfohint.ai_socktype = SOCK_STREAM;
	ainfohint.ai_protocol = IPPROTO_TCP;

	int error = getaddrinfo(remoteHost,port,&ainfohint,&ainfo);
	if (error!=0){
		std::cerr<<"ERROR: getaddrinfo() failed"<< gai_strerror(error)<<std::endl;
		exit(1); 
	}
	
	
	// ==== Convert remote addres to human-readable format ====
	char resolvedHost[NI_MAXHOST];
	char resolvedService[NI_MAXSERV];
	error = getnameinfo(ainfo->ai_addr,ainfo->ai_addrlen,(char*)&resolvedHost,sizeof(resolvedHost),(char*)&resolvedService,sizeof(resolvedService),NI_NUMERICHOST);
    if(error != 0){
    	std::cerr <<"ERROR: getnameinfo() failed: "<<gai_strerror(error)<<std::endl;
    	exit(1);
    }
    std::cout<<"Connecting to remote host "<< resolvedHost<<std::endl;
	// ==== Create socket of appropriate type ================

	// try out all sockets in the list until one connects
	struct addrinfo* ai = NULL;
	int con = -1;
    for (ai = ainfo; ai != NULL; ai = ai->ai_next)
    {
        sd = socket(ai->ai_family, ai->ai_socktype,ai->ai_protocol);
        if(sd<=0){
			continue;
		}
		con = connect(sd, ai->ai_addr, ai->ai_addrlen);
        if (con >= 0){
            break;
		}
    }

	if(sd<=0){
		perror("Unable to create socket");
		exit(1);
	}
	if(con < 0){
	perror("connect() call failed");
	exit(1);
	}
	signal(SIGINT, sigintHandler);
	char data[100];
	char req[10];
	char fileName[50];
	for(;;){
		
		bzero(data,100);
		bzero(req,10);
		bzero(fileName,50);
		
		char* result = NULL;
		result = fgets(data,100,stdin);
		
		if(result == NULL){
			perror("Error fgets()\n");
			exit(1);
		}
		
		sscanf(data,"%s %s",req,fileName);
		if(!strcmp(req,"Get")){
			getRequest(sd,fileName);
			receiveFile(sd);
		}
		if(!strcmp(req,"Quit")){
			quitRequest(sd);
		}

		if(!strcmp(req,"List")){
			listRequest(sd);
			receiveList(sd);
		}

		if(!strcmp(req,"Put")){
			put(sd,fileName);
		}

		if(!strcmp(req,"Files")){
			filesRequest(sd);
			filesReceive(sd);
		}
		fflush(stdout);
		fflush(stdin);
	}
		
    // ============================ Clean up =================================
    close(sd);
	freeaddrinfo(ainfo);
	return 0;
}