#define _XOPEN_SOURCE 700

#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <dirent.h>
#include <arpa/inet.h>
#include <fcntl.h> // for open
#include <unistd.h> // for close
#include <stdlib.h>
#include <ctype.h>
#include<pthread.h>
#include <errno.h>


#define ADDRESS "127.0.0.1"

char client_message[2000];
char server_message[2000];
char buffer[1024];
int read_directory(const char* directory);
void * socketThread(void *arg);
int port;
const char* directory;
char directory_list[20][1024]; // max directory list of 20 element strings, with size 1024 ea.
void close_connection(int socket);



int main(int argc, char *argv[]) {
    // error checking for arguments for running server.
    int i = 0;
    if(argc < 2)
    {
        printf("Directory name missing!\n");
        exit(EXIT_FAILURE);
    }else if(argc < 3)
    {
        printf("Port missing\n");
        exit(EXIT_FAILURE);
    } else if (!isdigit(argv[2][0])){
        printf("Port entered is not a number.\n");
        exit(EXIT_FAILURE);
    }
    port = atoi(argv[2]);
    directory = argv[1];
    int serverSocket, newSocket;
    struct sockaddr_in serverAddr;
    struct sockaddr_storage serverStorage;
    socklen_t addr_size;

    //init the directory list array to 0
    for (i=0; i < (sizeof( directory_list ) / sizeof( directory_list[0])); i++){
        strcpy(directory_list[i],"0");
    }

    printf("Port #: %d\n", port);int d;
    d = read_directory(directory);
    if (d != 0)
    {
        printf("We got a directory error read");
    }

    for ( i=0; i < 20; i++){
        if (strncmp(directory_list[i],"0", 1) != 0){
            printf("%s\n",  directory_list[i]);
        }
    }

    //Create the socket.
    if ((serverSocket = socket(AF_INET, SOCK_STREAM, 0)) < 0){
        perror("ERROR opening socket");
        exit(EXIT_FAILURE);
    }

    // Configure settings of the server address struct
    // Address family = Internet
    serverAddr.sin_family = AF_INET;

    //Set port number, using htons function to use proper byte order
    serverAddr.sin_port = htons(port);

    //Set IP address to localhost
    serverAddr.sin_addr.s_addr = inet_addr(ADDRESS);

    //Set all bits of the padding field to 0
    memset(serverAddr.sin_zero, '\0', sizeof serverAddr.sin_zero);

    //Bind the address struct to the socket
    if (bind(serverSocket, (struct sockaddr *) &serverAddr, sizeof(serverAddr)))
    {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    // Here the process will go in sleep mode and will wait/listen for the incoming connections from clients.
    // Listen on the socket, with 40 max connection requests queued
    if(listen(serverSocket,50) == -1){
        perror("listen");
        exit(EXIT_FAILURE);
    } else{
        printf("Listening\n");
    }

    pthread_t tid[60]; // the thread ID array.
    i = 0;
    while(1)
    {
        //Accept call creates a new socket for the incoming connection
        addr_size = sizeof serverStorage;
        newSocket = accept(serverSocket, (struct sockaddr *) &serverStorage, &addr_size);

        strcpy(buffer,"Hello World\n");
        send(newSocket,buffer,13,0);

        //for each client request creates a thread and assign the client request to it to process
        //so the main thread can entertain next request
        if( newSocket >= 0)
        {
            printf("user number %d \n", i);
            printf("Client is connected via socket: %d, starting new thread", newSocket);
            if( pthread_create(&tid[i], NULL, socketThread, &newSocket) != 0 )
                printf("Failed to create thread\n");
            i++;
        }
        else if( newSocket < 0)
            printf("Failed to connect");

        if( i >= 50)
        {
            i = 0;
            while(i < 50)
            {
                pthread_join(tid[i++],NULL);
            }
            i = 0;
        }
    }

    /**
    // read directory.
    printf("arg 1 -> %s\narg2 -> %s\narg3 -> %s\n", argv[0],argv[1], argv[2]);
    printf("Port #: %d\n", port);
    int d;
    d = read_directory(argv[1]);
    if (d != 0)
    {
        printf("We got a directory error read");
        return -1;
    }
     **/
    return 0;

}

/**
 * Start the directory list
 * https://stackoverflow.com/questions/4204666/how-to-list-files-in-a-directory-in-a-c-program - To help setup directory read.
 * @param directory the directory where the server is gonna have the files to serve clients.
 * @return int for Good or error.
 */
int read_directory(const char* directory){
    DIR *d;
    struct dirent *dir;
    //char cwd[256]; // + getcwd for the full directory name
    d = opendir(directory);
    if (d) {
        int i =0;
        while ((dir = readdir(d)) != NULL) {
            if (strcmp (dir->d_name, ".") == 0)
                continue;
            if (strcmp (dir->d_name, "..") == 0){
                continue;
            }
            //printf("%s\n", dir->d_name);
            strcpy(directory_list[i], dir->d_name);
            i++;
        }
        closedir(d);
    }else{
        perror("Couldn't open directory");
        return -1;
    }
    return 0;
}


void * socketThread(void *arg) {
    //char * Hello = "Hello from Server!";
    int newSocket = *((int *)arg);
    int server_read;
    // Send message to the client socket

    //printf("Connected to client <-----");
    //send(newSocket, Hello,strlen(server_message),0);

    if((server_read = read(newSocket, client_message, 20000) != 0)) {
        printf("Client Message: %s\n", client_message);
    }
   // while(strncmp(client_message, "00", 2) > 2)
    //{
        //for (int i =0; i < 20; i++) {
            //if (strncmp(directory_list[i], "0",1) != 0){
                //if (send(newSocket, Hello,strlen(server_message),0) == -1)
                  //  printf("error in sending");
           // }
       // }


        //if (recv(newSocket,client_message,2000,0) == 0)
            //printf("Error in Recv--\n");
       // else
        //    fputs(client_message,stdout);
    //}

    sleep(5);
    close_connection(newSocket);
    pthread_exit(NULL);

}

void close_connection(int socket){
    // Close the socket with a message to client and return to main thread handler to close and rejoin the thread.
    printf("Closing socket, and exiting Thread \n");
    char * Closing = "Terminating session.\n";
    if(send(socket, Closing, strlen(Closing), 0) == -1){
        printf("Error: %s\n", strerror(errno));
    }
    close(socket);
}