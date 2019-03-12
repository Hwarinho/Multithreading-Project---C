#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>

#define MAX_SIZE 4096
#define NUM_CLIENT 1
char buffer[1024];

void *connection_handler(void *socket_desc);

int main() {
    // from line 17 to 31 we only create sockets to server
    int sock_desc;
    struct sockaddr_in serv_addr;
    char sbuff[MAX_SIZE], rbuff[MAX_SIZE];

    if ((sock_desc = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        printf("Failed creating socket\n");


    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    serv_addr.sin_port = htons(9999);

    if (connect(sock_desc, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
        printf("Failed to connect to server\n");
    }
    recv(sock_desc, buffer, 1024, 0);
    printf("Data received: %s", buffer);

    while (1) {
        //in this loop we first listen to read from stdin and send, then we will get information from server to print
        if (fgets(sbuff, MAX_SIZE, stdin) == NULL) //reading from stdin
            printf("Error on reading from stdin");
        else if (send(sock_desc, sbuff, strlen(sbuff), 0) == -1)//sending
            printf("error in sending stdin");


        sleep(2);
        if (recv(sock_desc, rbuff, MAX_SIZE, 0) == 0)//reading from socket
            printf("Error in receiving");
        else
            fputs(rbuff, stdout); //print the information

        sleep(1);


    }
    close(sock_desc);
    return 0;
}
