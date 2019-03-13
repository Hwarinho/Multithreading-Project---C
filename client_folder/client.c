#define _XOPEN_SOURCE 700

#include <stdio.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>
#include <sys/types.h>
#include <fcntl.h>
#include <netdb.h>
#include <math.h>
#include <stdbool.h>
#include <ctype.h>


char buffer[BUFSIZ];
int port;
const char* address;
int filefd;
ssize_t read_return;
char server_message[BUFSIZ];

void *connection_handler(void *socket_desc);

void quit_session(int socket);
int upload_file(int socket, char *filename);
int download_file(int socket, char *filename);

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
    char client_message[BUFSIZ];
    if ((sock_desc = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        printf("Failed creating socket\n");
    // Configure settings of the client
    // Address family = Internet
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(address);
    serv_addr.sin_port = htons(9999);
    //Set IP address to localhost
    serv_addr.sin_addr.s_addr = inet_addr(address);
    //Set port number, using htons function to use proper byte order
    serv_addr.sin_port = htons(port);

    if (connect(sock_desc, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
        printf("Failed to connect to server\n");
    }
    recv(sock_desc, buffer, 1024, 0);
    printf("Data received: %s", buffer);

    while (1) {
        bool valid_message = true;

        //in this loop we first listen to read from stdin and send, then we will get information from server to print
        if (fgets(client_message, BUFSIZ, stdin) == NULL) {
            printf("Error on reading from stdin\n");
        }
        printf("Command is: ---%s", client_message);
        //Send message based on what the user has specified (l,u,d,r,q)
//----------------------- LIST ------------------------------------------------------//
        if (strncmp(client_message, "l\n", 2) == 0) {
            if (send(sock_desc, "0x00", strlen("0x00"), 0) == -1) {
                printf("Error in sending stdin\n");
            }
            //wait for responce from server thats not an empty message.
            while (1) {
                if (recv(sock_desc, server_message, BUFSIZ, 0) == 0) {
                    printf("Error in receiving\n");
                }
                // makes sure the server message is not empty, sometimes happens.
                if (strncmp(server_message, "0x", 2) == 0) {
                    printf("%s\n", server_message);
                    break;
                }
            }
            //for commands u, d, r, we need to remove user command letter, add the hexadecimal command to the string
            //and remove all but one whitespace characters following hexadecimal
//------------------------- UPLOAD ----------------------------------------------------------//
        } else if (strncmp(client_message, "u ", 2) == 0) {
            char string[100] = "";
            char *filename;

            strcat(string, "0x02 ");
            //use memmove to remove the first character of a string (for user command and whitespace characters)
            //code for memmove obtained from: https://stackoverflow.com/questions/4295754/how-to-remove-first-character-from-c-string
            memmove(client_message, client_message + 1, strlen(client_message));
            while (strncmp(client_message, " ", 1) == 0) {
                memmove(client_message, client_message + 1, strlen(client_message));
            }
            //send edited message
            if (send(sock_desc, strcat(string, client_message), BUFSIZ, 0) == -1) {
                perror("send error");
                exit(EXIT_FAILURE);
            }
            filename = strtok(client_message, "\n");
            filename = strtok(filename, " ");
            if (upload_file(sock_desc, filename) < 0) {
                fprintf(stdout, "error in uploading.");
            }
            continue;
//----------------------- DOWNLOAD ------------------------------------------------------//
        } else if (strncmp(client_message, "d ", 2) == 0) {
            char string[100] = "";
            char *filename;
            strcat(string, "0x06 ");
            memmove(client_message, client_message + 1, strlen(client_message));
            while (strncmp(client_message, " ", 1) == 0) {
                memmove(client_message, client_message + 1, strlen(client_message));
            }
            strcat(string, client_message);
            //send edited message
            if (send(sock_desc, string, strlen(string), 0) == -1)//sending
                printf("Error in sending stdin\n");
            filename = strtok(client_message, "\n");
            filename = strtok(filename, " ");

            if (download_file(sock_desc, filename) < 0) {
                fprintf(stdout, "error in downloading");
            }

            continue;
//----------------------- REMOVE ------------------------------------------------------//
        } else if (strncmp(client_message, "r ", 2) == 0) {
            char string[100] = "";
            strcat(string, "0x04 ");
            memmove(client_message, client_message + 1, strlen(client_message));
            while (strncmp(client_message, " ", 1) == 0) {
                memmove(client_message, client_message + 1, strlen(client_message));
            }
            strcat(string, client_message);
            //send edited message
            if (send(sock_desc, string, strlen(string), 0) == -1)
                printf("Error in sending stdin\n");
//----------------------- QUIT ------------------------------------------------------//
        } else if ((strncmp(client_message, "q\n", 2) == 0)) {
            if (send(sock_desc, "0x08", strlen("0x08"), 0) == -1) {
                printf("Error in sending stdin\n");
            }
            while (1) {
                if (recv(sock_desc, server_message, BUFSIZ, 0) == 0) {
                    printf("Error in receiving\n");
                } else {
                    if (strncmp(server_message, "0x09", 4) == 0) {
                        quit_session(sock_desc);
                        break;
                    }
                }
            }
        //else the program doesn't recognize commands of the user
        }else{
            printf("Invalid command.\n");
        }
    }
    close(sock_desc);
    return 0;
}


int upload_file(int socket, char *filename) {

    filefd = open(filename, O_RDONLY);
    if (filefd == -1) {
        perror("open");
        exit(EXIT_FAILURE);
    }

    while (1) {
        read_return = read(filefd, buffer, BUFSIZ);
        if (read_return == 0)
            break;
        if (read_return == -1) {
            perror("read");
            exit(EXIT_FAILURE);
        }
        if (write(socket, buffer, read_return) == -1) {
            perror("write");
            exit(EXIT_FAILURE);
        }
    }
    close(filefd);
    send(socket, "DONE", 4, 0);
    return 1;
}

int download_file(int socket, char *filename) {
    filefd = open(filename,
                  O_WRONLY | O_CREAT | O_TRUNC,
                  S_IRUSR | S_IWUSR);
    if (filefd == -1) {
        perror("open");
        exit(EXIT_FAILURE);
    }
    do {
        read_return = read(socket, buffer, BUFSIZ);
        if (strncmp(buffer, "DONE", 4) == 0) {
            fprintf(stdout, "Done with the file---\n");
            break;
        }
        printf("\n message from client is: %s", buffer);
        if (read_return == -1) {
            perror("read");
            exit(EXIT_FAILURE);
        }
        if (write(filefd, buffer, (size_t) read_return) == -1) {
            perror("write");
            exit(EXIT_FAILURE);
        }
    } while (read_return > 0);
    close(filefd);
    fprintf(stdout, "Download DONE------\n");
    return 1;
}

void quit_session(int socket) {
    close(socket);
    exit(EXIT_SUCCESS);
}