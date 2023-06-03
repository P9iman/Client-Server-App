#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

// TODO: Remove me.
#define SRV_PORT 7777
#define DEFAULT_PORT 0

void* handleList(void*arg);
void* handleFiles(void*arg);
void* handleGet(void*arg);
void* handlePut(void*arg);
void* handleQuit(void*arg);

int serverPort = 0;

int main(int argc, char** argv)
{
//	(void)argc;  // TODO: Remove cast and parse arguments.
//	(void)argv;  // TODO: Remove cast and parse arguments.

	//Port auslesen
	if(argc == 0)
	{
		serverPort = DEFAULT_PORT;
	}else
	{
		serverPort = (int)*(argv[0]);
	}

	int s_tcp, news;
	/**
	 * sa = die Socketadresse vom Server
	 * sa_client = die Socketadresse vom Client
	 */
	struct sockaddr_in sa, sa_client;
	//laenge der Socket-Adresse
	unsigned int sa_len = sizeof(struct sockaddr_in);
	char info[256];

	sa.sin_family = AF_INET;
	//setze die Portnummer in das struct für den Server
	sa.sin_port = htons(serverPort);

	sa.sin_addr.s_addr = INADDR_ANY;

	if ((s_tcp = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
	perror("TCP Socket");
	return 1;
	}

	if (bind(s_tcp, (struct sockaddr*)&sa, sa_len) < 0) {
	perror("bind");
	return 1;
	}

	if (listen(s_tcp, 5) < 0) {
	perror("listen");
	close(s_tcp);
	return 1;
	}
	// TODO: Check port in use and print it.
	printf("Waiting for TCP connections ... \n");

	while (1) {
	if ((news = accept(s_tcp, (struct sockaddr*)&sa_client, &sa_len)) < 0) {
	  perror("accept");
	  close(s_tcp);
	  return 1;
	}
	if (recv(news, info, sizeof(info), 0)) {
	  printf("Message received: %s \n", info);
	}
	}

	close(s_tcp);
}
