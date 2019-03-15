/* Created MARCH 2 2019, by NIKITA YEVTUKH 
* CS379 ASSIGNMENT 2. Run by ./main [directory] [port number].
* More information can be found in README.
*/
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
#include <signal.h>
#include <sys/syscall.h>
//#include <openssl/md5.h>
#include <sys/sendfile.h>
#include <curses.h>

#define ADDRESS "127.0.0.1"

// Defining globals and functions.
int filefd;
ssize_t read_return;
char client_message[2000];
char server_message[2000];
char buffer[BUFSIZ];
int read_directory(const char* directory);
void * socketThread(void *arg);
int port;
int serverSocket;
int number_of_files = 0;
const char* directory;
char directory_list[30][1024]; // max directory list of 20 element strings, with size 1024 ea.
void close_connection(int socket);
int list_repository(int socket_id);
int md5Hash(char *filename);
int download_file(int socket, char *filename);
int upload_file(int socket, char *filename);
void SigHandler(int signo);
bool quit_flag_client = false;
bool quit_flag_server = false;
volatile sig_atomic_t done = 0; //atomic read and writes.


/** This is the main function that takes in directory and port number as arguments.
 * It starts by checking arguments for errors. Then proceeds to start a SIGTERM handler.
 * It then populates a global list to store file names that are in the directory requested.
 * Lastly it sets up a TCP socket connection that holds 40 sockets and 60 threads, each new connection
 * gets a socket and thread were the client commands are run. upon termination this should be closeing the 
 * threads to avoid zombie threads. (My attempt at that).
 **/
int main(int argc, char *argv[]) {
    // error checking for arguments for running server.
    int i = 0;
    if(argc < 2)
    {
        perror("Directory name missing!\n");
        exit(EXIT_FAILURE);
    }else if(argc < 3)
    {
        printf("Port missing\n");
        exit(EXIT_FAILURE);
    } else if (!isdigit(argv[2][0])){
        printf("Port entered is not a number.\n");
        exit(EXIT_FAILURE);
    }
    // setting up the signal handler for sigterm & sigint. Modified from assignemnt 1.
    static struct sigaction sa;
    sa.sa_handler = SigHandler;
	sigemptyset(&sa.sa_mask);
    sa.sa_flags =0;
	sa.sa_flags = SA_RESTART;
    
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);
    // set up socket stuff based on user arguments.
    port = atoi(argv[2]);
    directory = argv[1];
    int newSocket;
    struct sockaddr_in serverAddr;
    struct sockaddr_storage serverStorage;
    socklen_t addr_size;

    //init the directory list array to 0
    for (i=0; i < (sizeof( directory_list ) / sizeof( directory_list[0])); i++){
        strcpy(directory_list[i],"0");
    }
    if ((read_directory(directory)) < 0) {
        perror("read directory");
        exit(EXIT_FAILURE);
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

/** This function is gonna handle each individual thread logic. So each command a client issues on its thread will be processed here.
 * This function also takes care if a client quits and thus closes its socket and exits the thread.
 * The main commands for clients are Upload, Delete, List, Download and Quit.
 **/
void * socketThread(void *arg) {
    int newSocket = *((int *)arg);
    // Send message to the client socket
    while (!quit_flag_client){
        // Waiting for client input, reset to fresh buffers
        memset(buffer,0,BUFSIZ);
        memset(client_message, 0, 2000);
        if((read(newSocket, client_message, BUFSIZ) != 0)) {
            printf("Client Message: %s\n", client_message);
        }else{
            // in case of "bad"/glitchy clients, don't deal with them and close the socket and thread await new connection.
            perror("error in reading client message!");
            close(newSocket);
            quit_flag_client = TRUE;
            break;
        }
        // As of Now it can recognize the commands with other text.
        if (strncmp(client_message, "0x", 2) != 0) {
            if ((send(newSocket, "0xFF Invalid Command\n", 22, 0)) == -1) {
                fprintf(stdout, "error in sending");
            }
        } else {
            if (strncmp(client_message, "0x08", 4) != 0) {
//----------------------------- LIST --------------------------------------------------//
                if (strncmp(client_message, "0x00", 4) == 0) {
                    // function to print out directory, server response code 0x01
                    
                    if ((read_directory(directory)) < 0) {
                        perror("read directory");
                        if(send(newSocket,"0xFF",4,0) == -1){
                            perror("send");
                            continue;
                        }
                    }
                    // to get an up-to-date list.
                    if ((list_repository(newSocket)) < 0) { 
                        // server response for 0xFF and error in sending repo list.
                        char *error = "error in sending list.\n";
                        if(send(newSocket,error,strlen(error),0)){
                            perror("send");
                            continue;
                        }
                    }         
                    continue;
//----------------------------- UPLOAD --------------------------------------------------//
                // Add timeout of 15 seconds here. If time expires sends client and 0xFF and awaits new command.
                } else if (strncmp(client_message, "0x02", 4) == 0) {
                    char *filename;
                    // Grabs the file name from incoming message.
                    memmove(client_message, client_message + 5, strlen(client_message));
                    filename = strtok(client_message, "\n");
                    filename = strtok(filename, " ");
                    // checks if the filename is already in the repo.
                    for (int i = 0; i < 30; i++) {
                        if (strcmp(filename, directory_list[i]) == 0) {
                            printf("Found file already in repo... %s--%s\n", filename, directory_list[i]);
                            break;
                        }
                    }
                    // downloads the clients file. The mutex lock goes here.
                    if ((download_file(newSocket, filename)) < 0) {
                        perror("0xFF download");
                        continue;
                    }
                    // exit lock here.
                    //md5Hash(file_path);
                    continue;

//----------------------------- Delete --------------------------------------------------//
                } else if (strncmp(client_message, "0x04", 4) == 0) {
                // Add timeout of 15 seconds here. If time expires sends client and 0xFF and awaits new command.
                    // Lock goes here for the deletion process. This is a pretty trivial task just ran out of time cuz of soloing the project.
                    // function to delete file and server response code 0x05
                    continue;
//----------------------------- DOWNLOAD --------------------------------------------------//
                } else if (strncmp(client_message, "0x06", 4) == 0) {
                // Add timeout of 15 seconds here. If time expires sends client and 0xFF and awaits new command.
                    char *filename;
                    memmove(client_message, client_message + 5, strlen(client_message));
                    filename = strtok(client_message, "\n");
                    filename = strtok(filename, " ");
                    // Lock would go here for all threads.
                    if (upload_file(newSocket, filename) < 0) {
                        perror("upload error");
                        sleep(1);
                        if (send(newSocket,"0xFF",4,0) == -1){
                            perror("send");
                            continue;
                        }
                        continue;
                    }
                    // need the sleep for timeing issue of terminating the stream of data to client. I know it's not effiecent.
                    sleep(1);
                    if (send(newSocket,"0x07", 4, 0) ==-1){
                        perror("send");
                        continue;
                    }
                    // exit lock.
                    continue;
                }
            } else {
//----------------------------- QUIT --------------------------------------------------//
                send(newSocket, "0x09", 4, 0);
                quit_flag_client = true;
                break;
            }
        }
    }
    // close connection and thread and await new connection.
    close_connection(newSocket);
    quit_flag_client = false;
    pthread_exit(NULL);
}

/**
 * Safe Close given socket and exit the thread.
 * @param socket
 */
void close_connection(int socket){
    // Do shutdown of server socket, this is where it should also do save the XML but never implememnted.
    printf("Closing socket, and exiting Thread \n");
    shutdown(socket,2);
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
    //fprintf(stdout, "%d--number of files\n", number_of_files);
    char string[1000] = "";
    strcat(string, "0x01 ");
    strcat(string, str);
    strcat(string, " ");
    free(str);
    char *buffer2 = NULL;
    for (int i = 0; i < number_of_files; i++) {
        if (strncmp(directory_list[i], "0", 1) != 0) {
            //printf("%s\n", directory_list[i]);
            buffer2 = malloc(sizeof(buffer2) + sizeof(directory_list[i]) + 1);
            strcpy(buffer2, directory_list[i]);
            strcat(buffer2, " ");
            strcat(string, buffer2);
        }
    }
    free(buffer2);
    //printf("String so far: %s\n", string);
    strcpy(server_message, string);
    if ((send(socket_id, server_message, 2000, 0)) == -1) {
        //error in sending.
        return -1;
    }
    // clear server message
    strcpy(server_message,"");
    return 1;
}

/** This handles the download from server to client, this gets the file size and uses it to write to socket in 8192 size chunks until all
 *  the size is done then sends the termination signal to client(the ok message).
 **/
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
        //printf("THE FILE SIZE IS: %zd\n",size);
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
		            }
                if (ferror(fp))
                    printf("Error reading\n");
                break;
            }
        }
    return 1;
}
/** This handles the upload from client to server, this gets the file size and uses it to write to socket in 8192 size chunks until all
 *  the size is done then sends the termination signal to client(the ok message). Sorry if it is a bit confusing with the names, made sense
 * in my head at the time.
 **/
int download_file(int socket, char *filename) {
    memset(buffer,0,BUFSIZ);
    filefd = open(filename,
                  O_WRONLY | O_CREAT | O_TRUNC,
                  S_IRUSR | S_IWUSR);
    if (filefd == -1) {
        perror("open");
        return -1;
    }
    memset(client_message,0,2000);
    do {
        read_return = read(socket, buffer, BUFSIZ);
        //printf("BYTES RECIVED: %d\n", (int) read_return);
        if((read_return == 4) && (strncmp(buffer,"0xFF",4)==0)){
            printf("%s Server couldn't find file\n",buffer);
            close(filefd);
            remove(filename);
            return -1;
        }else if((read_return == 4) && (strncmp(buffer,"0x03",4)==0)){
            fprintf(stdout,"DONE");
            close(filefd);
            return 1;
        }else if((read_return == 8192) && (strncmp(buffer,"0x02", 4)==0)){
            // had a problem with the read recive where it would get a full buffer of gibberish, could find out why so just made a condition.
            memset(buffer, 0, BUFSIZ);
            continue;
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
            continue;
        }
        // reset buffer for fresh read.
        memset(buffer,0,BUFSIZ);
    } while (read_return > 0);
    close(filefd);
    return 1;
}
/** Handles incomming signal and closes the socket and then quits server. **/
void SigHandler(int signo){
    // Idea for Graceful termination. https://stackoverflow.com/questions/9681531/graceful-shutdown-server-socket-in-linux.
    // The idea behind is we need to traverse all tid's of all threads open and close their sockets, then join them all to avoid zombie
    // Threads and then print the XML .dedup file. However I had trouble finding out how to traverse the thread tid's and Oleg never made the xml
    // writer or parser. So the handler just closes the server side socket which then based on this link: https://stackoverflow.com/questions/11043128/should-java-streams-be-closed-before-the-backing-socket-is-closed/11043520#11043520.
    // the OS is capable of then closing the stream from the TCP connection and thus freeing up the space and socket.
    if ((signo == SIGTERM) || (signo == SIGINT)){
        fprintf(stdout, "Starting safe shutdown..\n");
        shutdown(serverSocket, 2);
        exit(EXIT_SUCCESS);

    }
    perror("Error in gracful termination");
    exit(EXIT_FAILURE);
}
/* This was start of Olegs implementation of md5 hash of the file contents, he never fisnished beyond this.
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