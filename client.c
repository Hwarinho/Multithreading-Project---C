/* Created MARCH 2 2019, by NIKITA YEVTUKH MODIFED by Oleg Krushenyysky.
* CS379 ASSIGNMENT 2. Run by ./client [address] [port number].
* More information can be found in README.
*/
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

// Defining globals and functions.
char buffer[BUFSIZ];
int port;
const char* address;
int filefd;
ssize_t read_return;
char server_message[4096];
void *connection_handler(void *socket_desc);
void quit_session(int socket);
int upload_file(int socket, char *filename);
int download_file(int socket, char *filename);


/**
 * This is the driver main, takes net address and port as arguments.
 * It first does error checking for the arguments then proceeds to establish connection
 * to the server via specified address and port. There are no threads here for simplicity.
 * After the connection is established (if no errors occur), it proceeds to await input from 
 * stdin in the following commands l (list repo of server), u FILENAME (to server), r FILENAME (remove from server ** NOT IMPLEMENTED**),
 * d filename (download from server), q (quit connection). Does basic error checking and signals from server to see what happened which are printed 
 * to stdout and in some cases even terminates the client.
 **/
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
        //Send message based on what the user has specified (l,u,d,r,q), and clear server_message and buffer.
        strcpy(buffer,"");
        strcpy(server_message, "");
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
                    strcpy(server_message, "");
                    break;
                }
            }
            //for commands u, d, r, we need to remove user command letter, add the hexadecimal command to the string
            //and remove all but one whitespace characters following hexadecimal
//------------------------- UPLOAD ----------------------------------------------------------//
        } else if (strncmp(client_message, "u ", 2) == 0) {
            char string[100] = "";
            char *filename;
            // Clear server_message and buffer;
            strcpy(buffer,"");
            strcpy(server_message,"");
            // Handle if command doesn't have secondary requirements.
            if (strncmp(client_message, "u  ", 3) == 0) {
                fprintf(stdout, "Need filename.\n");
                continue;
            }
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
                break;
            }
            // needed for timing of socket messages.
            sleep(1);
            if (send(sock_desc,"0x03", 4, 0) ==-1){
                perror("send");
                continue;
            }
            // success based on the 
            fprintf(stdout, "OK\n");
           continue;
//----------------------- DOWNLOAD ------------------------------------------------------//
        } else if (strncmp(client_message, "d ", 2) == 0) {
            // Handle if command doesn't have secondary requirements.
            if (strncmp(client_message, "d  ", 3) == 0) {
                fprintf(stdout, "Need filename.\n");
                continue;
            }
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

            if ((download_file(sock_desc, filename)) < 0) {
                continue;
            }
            // If it got here based on the error checking its safe to just print the ok message.
            fprintf(stdout, "OK\n");
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
                char quiter[4];
                // reset server message.
                memset(server_message,0,2000);
                if (recv(sock_desc, quiter, 4, 0) == 0) {
                    printf("Error in receiving\n");
                } else {
                    if (strncmp(quiter, "0x09", 4) == 0) {
                        fflush(stdout);
                        printf("OK\n");
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
    // Used help with how to set up the while loop and buffer to send the file contents to socket.
    // https://www.youtube.com/watch?reload=9&v=12F3GBw28Lg.
    FILE *fp = fopen(filename,"rb");
        if(fp==NULL){
            perror("open");
            return -1;   
        }   
        /* Read data from file and send it */
        struct stat buf;
        stat(filename, &buf);
        off_t size = buf.st_size;
        printf("THE FILE SIZE IS: %zd\n",size);
        printf("THIS IS THE BUFFER BEFORE:--%s",buffer);
        memset(buffer,0,BUFSIZ);
        while(1)
        {
            unsigned char buff[BUFSIZ];
            int nread = fread(buff,1,BUFSIZ,fp);
            /* If read was success, send data. */
            if(nread > 0)
            {
                size -= nread;
                printf("THE NEW SIZE IS BLAH: %zd\n",size);
                write(socket, buff, nread);
            }
            if (size <= 0){
                if (feof(fp)){
                    printf("End of file\n");
                    if(send(socket,"",0,0)== -1){
                        perror("send");
                        return -1;
                    }
		            printf("File transfer completed for id: %d\n",socket);
		            }
                if (ferror(fp))
                    printf("Error reading\n");
                break;
            }
        }
    return 1;
}




int download_file(int socket, char *filename) {
    memset(buffer,0,BUFSIZ);
    filefd = open(filename,
                  O_WRONLY | O_CREAT | O_TRUNC,
                  S_IRUSR | S_IWUSR);
    if (filefd == -1) {
        perror("open");
        return -1;
    }
    do {
        read_return = read(socket, buffer, BUFSIZ);
        printf("BYTES RECIVED: %d and string--%s\n", (int) read_return, buffer);
        if((read_return == 4) && (strncmp(buffer,"0xFF",4)==0)){
            printf("%s Server couldn't find file\n",buffer);
            close(filefd);
            remove(filename);
            return -1;
        }else if((read_return == 4) && (strncmp(buffer,"0x07",4)==0)){
            printf("%s\n", buffer);
            close(filefd);
            return 1;
        }
        if (read_return == -1) {
            perror("read");
            return -1;
        }
        if (write(filefd, buffer, read_return) == -1) {
            perror("write");
            return -1;
        }
        if (read_return == 0){
            fprintf(stdout, "READ RETURN IS NOW ZERO< STOP WRITING.");
            continue;
        }
        // reset buffer for fresh read.
        memset(buffer,0,BUFSIZ);
    } while (read_return > 0);
    close(filefd);
    return 1;
}
/**
 * This quits the clients session and closes the socket.
 **/
void quit_session(int socket) {
    close(socket);
    exit(EXIT_SUCCESS);
}