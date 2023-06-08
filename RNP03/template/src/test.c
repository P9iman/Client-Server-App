#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#include <stdio.h>

int main(int argc, char const *argv[])
{
    getaddrinfo(NULL, NULL, NULL, NULL); 


    printf("Hallo\n"); 
    /* code */
    return 0;
}
