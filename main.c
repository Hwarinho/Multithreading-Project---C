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
#include <pthread.h>
#include <errno.h>
#include <stdbool.h>
#include <math.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <netdb.h>
//#include <openssl/md5.h>
//#include <sys/sendfile.h>
//#include <linux/list.h>

#define ADDRESS "127.0.0.1"
int filefd;
ssize_t read_return;


char client_message[2000];
char server_message[2000];
char buffer[BUFSIZ];
int read_directory(const char* directory);
void * socketThread(void *arg);
int port;
int number_of_files = 0;
const char* directory;
char directory_list[30][1024]; // max directory list of 20 element strings, with size 1024 ea.
void close_connection(int socket);
int list_repository(int socket_id);
int md5Hash(char *filename);

int upload_file(int socket, char *filename);
bool quit_flag_client = false;
bool quit_flag_server = false;



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
    printf("Port #: %d\n", port);

    if ((read_directory(directory)) < 0) {
        printf("We got a directory error read");
    }

    //Create the socket.

    if ((serverSocket = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
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
    if (bind(serverSocket, (struct sockaddr *) &serverAddr, sizeof(serverAddr))) {
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
    while(!quit_flag_server)
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
            printf("Client is connected via socket: %d, starting new thread\n", newSocket);
            fflush(stdout);
            // as soon as client sends a message a new thread is made to deal with the request and the server goes back into sleep mode.
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

    // TODO kill threads and join, then grab and save state of XML for file struct. Then quit.
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
            strcpy(directory_list[i], dir->d_name);
            i++;
        }
        closedir(d);
        number_of_files = i;
    }else{
        // Couldn't open directory asked.
        return -1;
    }
    return 0;
}


void * socketThread(void *arg) {
    int newSocket = *((int *)arg);
    // Send message to the client socket
    fflush(stdout);
    fflush(stdin);

    while (!quit_flag_client){
        fprintf(stdout, "Waiting for client input..........\n");

        if((read(newSocket, client_message, 20000) != 0)) {
            printf("Client Message: %s\n", client_message);
        }else{
            perror("error in reading client message!");
            //exit(EXIT_FAILURE);
        }
        //fflush(stdout);
        //fflush(stdin);
        // TODO client message preprocessing for instruction list.
        //strcpy(client_message, strtok(client_message, "\n")); //for stripping null term
        // As of Now it can recognize the commands with other text.

        if (strncmp(client_message, "0x", 2) != 0) {
            if ((send(newSocket, "0xFF Invalid Command\n", 22, 0)) == -1) {
                fprintf(stdout, "error in sending");
            }
        } else {
            if (strncmp(client_message, "0x08", 4) != 0) {
//----------------------------- LIST --------------------------------------------------//
                if (strncmp(client_message, "0x00", 4) == 0) {
                    fprintf(stdout, "--printing directory---\n");
                    // function to print out directory, server response code 0x01
                    if ((list_repository(newSocket)) > 0) {  // directory_list is a global list.
                        fprintf(stdout, "No Error");
                    } else {
                        // server response for 0xFF and error in sending repo list.
                        fprintf(stdout, "was a error");
                    }
                    continue;
//----------------------------- UPLOAD --------------------------------------------------//
                } else if (strncmp(client_message, "0x02", 4) == 0) {
                    fprintf(stdout, "Client is uploading file\n");

                    char *filename;

                    memmove(client_message, client_message + 5, strlen(client_message));
                    filename = strtok(client_message, "\n");
                    filename = strtok(filename, " ");
                    //strcat(filename,"2"); --- test for difference.
                    printf("The file name being open is: %s\n", filename);

                    filefd = open(filename,
                                  O_WRONLY | O_CREAT | O_TRUNC,
                                  S_IRUSR | S_IWUSR);
                    if (filefd == -1) {
                        perror("open");
                        exit(EXIT_FAILURE);
                    }
                    do {
                        read_return = read(newSocket, buffer, BUFSIZ);
                        // To see if the upload is done.
                        if (strncmp(buffer, "DONE", 4) == 0) {
                            fprintf(stdout, "Done with the file---\n");
                            break;
                        }
                        if (read_return == -1) {
                            perror("read socket");
                            exit(EXIT_FAILURE);
                        }
                        if (write(filefd, buffer, (size_t) read_return) == -1) {
                            perror("write");
                            exit(EXIT_FAILURE);
                        }
                    } while (read_return > 0);
                    close(filefd);
                    fprintf(stdout, "UPLOAD DONE------\n");

                    //ALL CAPS THIS IS WHERE THE MD5HASH IS LOOK FOR IT HERE
                    //md5Hash(file_path);
                    //function for handling upload and file checking. server response code 0x03

                    continue;
//----------------------------- Delete --------------------------------------------------//
                } else if (strncmp(client_message, "0x04", 4) == 0) {
                    fprintf(stdout, "Client wants to delete a file\n");
                    // function to delete file and server response code 0x05
                    continue;
//----------------------------- DOWNLOAD --------------------------------------------------//
                } else if (strncmp(client_message, "0x06", 4) == 0) {
                    fprintf(stdout, "Client wants to download a file\n");
                    // -- Lock might go here.----
                    // need to send it as: 0x07 | size | contents|
                    char *filename;

                    memmove(client_message, client_message + 5, strlen(client_message));
                    filename = strtok(client_message, "\n");
                    filename = strtok(filename, " ");
                    if (upload_file(newSocket, filename) < 0) {
                        perror("upload error");
                        exit(EXIT_FAILURE);
                    }
                    send(newSocket, "DONE", 4, 0);
                    continue;
                }
                fprintf(stdout, "-----------Client has quit\n");
            } else {
//----------------------------- QUIT --------------------------------------------------//
                fprintf(stdout, "Client message was quit\n");
                send(newSocket, "0x09", 4, 0);
                quit_flag_client = true;
                break;
            }
        }

    }
    close_connection(newSocket);
    quit_flag_client = false;
    pthread_exit(NULL);

}

/**
 * Safe Close given socket.
 * @param socket
 */
void close_connection(int socket){
    // Close the socket with a message to client and return to main thread handler to close and rejoin the thread.
    printf("Closing socket, and exiting Thread \n");
    char * Closing = "Terminating session.\n";
    if(send(socket, Closing, strlen(Closing), 0) == -1){
        printf("Error: %s\n", strerror(errno));
    }
    close(socket);
}

/**
 * gather directery and combine them into the required string to send as on message. 0x01 | N | filename1 |\0| filename2 |\0| ... | filenameN |\0|.
 * @param socket_id the socket to send directory list to.
 * @return success or errors based on errno or ints.
 */
int list_repository(int socket_id) {
    // used from https://stackoverflow.com/questions/8257714/how-to-convert-an-int-to-string-in-c to get int to string concat.
    int length = snprintf(NULL, 0, "%d", number_of_files);
    char *str = malloc((size_t) sizeof((length + 1)));
    snprintf(str, length + 1, "%d", number_of_files);
    fprintf(stdout, "%d--number of files\n", number_of_files);
    char string[1000] = "";
    strcat(string, "0x01 ");
    strcat(string, str);
    strcat(string, " ");
    free(str);
    char *buffer2 = NULL;
    for (int i = 0; i < number_of_files; i++) {
        if (strncmp(directory_list[i], "0", 1) != 0) {
            printf("%s\n", directory_list[i]);
            buffer2 = malloc(sizeof(buffer2) + sizeof(directory_list[i]) + 1);
            strcpy(buffer2, directory_list[i]);
            strcat(buffer2, " ");
            strcat(string, buffer2);
        }
    }
    free(buffer2);
    printf("String so far: %s\n", string);
    strcpy(server_message, string);
    if ((send(socket_id, server_message, 2000, 0)) == -1) {
        //error in sending.
        return -1;
    }
    return 1;
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
            perror("read---filefd");
            exit(EXIT_FAILURE);
        }
        if (write(socket, buffer, read_return) == -1) {
            perror("write");
            exit(EXIT_FAILURE);
        }
    }
    close(filefd);
    return 1;
}
/*
int md5Hash(char *filename){
    unsigned char c[MD5_DIGEST_LENGTH];
    int i;
    FILE *inFile = fopen (filename, "rb");
    MD5_CTX mdContext;
    int bytes;
    unsigned char data[1024];

    if (inFile == NULL) 
	{
        printf ("%s can't be opened.\n", filename);
        return 0;
    }

    MD5_Init (&mdContext);
    while ((bytes = fread (data, 1, 1024, inFile)) != 0)
        MD5_Update (&mdContext, data, bytes);
    MD5_Final (c,&mdContext);
    for(i = 0; i < MD5_DIGEST_LENGTH; i++) printf("%02x", c[i]);
    printf("\n");
    fclose (inFile);
    return 0;
}
 */