##include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <math.h>
#include <stdbool.h>
//included to remove the warning from isdigit()
#include <ctype.h>

#define MAX_SIZE 4096
#define NUM_CLIENT 1
#define ADDRESS "127.0.0.1"
char buffer[1024];
int port;
const char* address;

void *connection_handler(void *socket_desc);

int main(int argc, char *argv[]) {
    //checking if client specified address and port
    if(argc < 2){
        printf("Address missing!\n");
        exit(EXIT_FAILURE);
    }else if(argc < 3){
        printf("Port missing\n");
        exit(EXIT_FAILURE);
    } else if (!isdigit(argv[2][0])){
        printf("Port entered is not a number.\n");
        exit(EXIT_FAILURE);
    }
    
    port = atoi(argv[2]);
    address = argv[1];
    // from line 17 to 31 we only create sockets to server
    int sock_desc;
    struct sockaddr_in serv_addr;
    char clientMessage[MAX_SIZE], rbuff[MAX_SIZE];

    if ((sock_desc = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        printf("Failed creating socket\n");

    // Configure settings of the client
    // Address family = Internet
    serv_addr.sin_family = AF_INET;
    //Set IP address to localhost
    serv_addr.sin_addr.s_addr = inet_addr(address);
    //Set port number, using htons function to use proper byte order
    serv_addr.sin_port = htons(port);

    if (connect(sock_desc, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
        printf("Failed to connect to server\n");
    }
    recv(sock_desc, buffer, 1024, 0);
    printf("Data received: %s\n", buffer);

    while (1) {
        bool valid_message = true;

        //in this loop we first listen to read from stdin and send, then we will get information from server to print
        if (fgets(clientMessage, MAX_SIZE, stdin) == NULL) //reading from stdin
            printf("Error on reading from stdin\n");


        //Send message based on what the user has specified (l,u,d,r,q)
        if (strncmp(clientMessage, "l", 1) == 0){
            if (send(sock_desc, "0x00", strlen("0x00"), 0) == -1)//sending
                printf("Error in sending stdin\n");
        //for commands u, d, r, we need to remove user command letter, add the hexadecimal command to the string 
        //and remove all but one whitespace characters following hexadecimal 
        }else if(strncmp(clientMessage, "u ", 2) == 0){
            char string[1000] = "";
            strcat(string, "0x02 ");
            //use memmove to remove the first character of a string (for user command and whitespace characters)
            //code for memmove obtained from: https://stackoverflow.com/questions/4295754/how-to-remove-first-character-from-c-string
            memmove(clientMessage, clientMessage+1, strlen(clientMessage));
            while(strncmp(clientMessage," ",1)==0){
                memmove(clientMessage, clientMessage+1, strlen(clientMessage));
            }
            strcat(string, clientMessage);
            //send edited message
            if (send(sock_desc, string, strlen(string), 0) == -1)//sending
                printf("Error in sending stdin\n");
        }else if(strncmp(clientMessage, "d ", 2) == 0){
            char string[1000] = "";
            strcat(string, "0x04 ");
            memmove(clientMessage, clientMessage+1, strlen(clientMessage));
            while(strncmp(clientMessage," ",1)==0){
                memmove(clientMessage, clientMessage+1, strlen(clientMessage));
            }
            strcat(string, clientMessage);
            //send edited message
            if (send(sock_desc, string, strlen(string), 0) == -1)//sending
                printf("Error in sending stdin\n");
            
        }else if(strncmp(clientMessage, "r ", 2) == 0){
            char string[1000] = "";
            strcat(string, "0x06 ");
            memmove(clientMessage, clientMessage+1, strlen(clientMessage));
            while(strncmp(clientMessage," ",1)==0){
                memmove(clientMessage, clientMessage+1, strlen(clientMessage));
            }
            strcat(string, clientMessage);
            //send edited message
            if (send(sock_desc, string, strlen(string), 0) == -1)//sending
                printf("Error in sending stdin\n");
        }else if((strncmp(clientMessage, "q", 1) == 0)){
            if (send(sock_desc, "0x08", strlen("0x08"), 0) == -1)//sending
                printf("Error in sending stdin\n");
        //else the program doesn't recognize commands of the user
        }else{
            valid_message = false;
            printf("Invalid command.\n");
        }

        if (valid_message ==true){
            if (recv(sock_desc, rbuff, MAX_SIZE, 0) == 0)//reading from socket
            printf("Error in receiving\n");
            else
            fputs(rbuff, stdout); //print the information
            fputs("\n", stdout);

        }
    }
    close(sock_desc);
    return 0;
}
