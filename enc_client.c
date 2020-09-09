#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <netdb.h>
#include <fcntl.h>
#include <sys/select.h>
#include <ctype.h>


//Function: recvAll
//--------------------------
//Description: Receives all data from a send call over a socket and saves it in a buffer
//Returns: N/A
void recvAll(int client_socket_fd, char message_buffer[], int bufferSize){

    char *buffer = message_buffer;
    int recvSize;
    int sizeLeft;


    //Store the size of the message which is about to be received
    sizeLeft = bufferSize;

    //Loop until we have received the full message from the server
    while(sizeLeft > 0){

        if((recvSize = recv(client_socket_fd, buffer, sizeLeft, 0)) == -1){
            fprintf(stderr, "Error: Cannot read data from socket\n");
            exit(1);
        }

        if (recvSize == 0){
            fprintf(stdout,"All bytes received\n");
            break;
        }

        sizeLeft -= recvSize;
        buffer += recvSize;
    }

}


//Function: sendAll
//--------------------------
//Description: Sends all data to the server, even for larger files
//Returns: N/A
void sendAll(int client_socket_fd, char message_to_send[]){


    char *buffer = message_to_send;
    int sendSize = 0;
    int sizeLeft;

    // Keep track of how big the message is we are trying to send, so we ensure the whole message is sent
    sizeLeft = strlen(message_to_send);

    //Loop until we've sent the whole message over the socket
    while(sizeLeft > 0){

        if((sendSize = send(client_socket_fd, buffer, sizeLeft, 0)) == -1){
            fprintf(stderr, "Error: Cannot send data over the socket\n");
            exit(1);
        }

        sizeLeft -= sendSize;
        buffer += sendSize;

    }

}


//Function: readFile
//--------------------------
//Description: Generic function which reads data from a file and stores it in the passed in buffer
//Returns: N/A
char *readFile(const char *fileName, char textBuffer[]){

    FILE *fp;
    char buffer[100000];
    int fileSize = 0;

    //Creates a new file descriptior and opens the file passed in
    fp = fopen(fileName, "r");

    fgets(buffer, sizeof(buffer),fp);
    fileSize = strlen(buffer);

    //Make sure that we remove any new line char from the end of the file for the grading script
    buffer[fileSize - 1] = '\0';


    //Check for bad data. Only spaces, capital letters are allowed.
    for (int i = 0; i < strlen(buffer); i++){
        if(buffer[i] == ' '){
            continue;
        }
        else if (!isupper(buffer[i]) || isdigit(buffer[i])){
            fprintf(stderr, "ERROR: Bad data in file %s\n",fileName);
            exit(1);
        }
    }

    //Copy the data from the file to the buffer passed in
    strcpy(textBuffer, buffer);

    //Close the file descriptor
    fclose(fp);

}


//Function: concatPlaintextAndKey
//--------------------------
//Description: Function combines the plaintext and the key strings together and prepares them to be sent over the socket
//Returns: N/A
char *concatPlaintextAndKey(char plaintext[], char key[], char messageToSendBuffer[]){

    char *termChar = "@@";
    char buffer[200000];

    strcpy(buffer, plaintext);
    //Concat plaintext and key and seperate them by a custom delimiter @@
    //This will be used to seperate the plaintext from the key on the server
    strcat(buffer, "@@");
    strcat(buffer, key);
    strcat(buffer, termChar);

    strcpy(messageToSendBuffer, buffer);
//    printf("%s\n", buffer);

}


int main(int argc, char *argv[]) {

    //If no port number is provided, notify the user and end the program
    if (argc < 3){
        fprintf(stderr, "Insufficient number of arguments\n", argv[0]);
        exit(1);
    }

    int client_socket_fd, charsWritten, strSize;
    int portNumber = atoi(argv[3]);
    const char *hostName = "localhost";
    char buffer[100000];
    char plainTextBuffer[100000];
    char keyBuffer[100000];
    char messageToSendBuffer[100000];
    char cipherFromServerBufer[200000];
    const char *plaintext = argv[1];
    const char *key = argv[2];
    //create a socket address. A port number and an IP address help us identify a specific process on a machine
    struct sockaddr_in serverAddress;
    //data type


//-------------------------------------------------------
// Handle plain text and key files
//-------------------------------------------------------

    //Get plaintext file contents
    readFile(plaintext, plainTextBuffer);

    //Get key from file;
    readFile(key,keyBuffer);

    //Check to ensure key length of the key is at least as large as the plaintext

    if (strlen(keyBuffer) < strlen(plainTextBuffer)){
        fprintf(stderr, "Error: key '%s'is to short\n", keyBuffer);
        exit(1);
    }

    memset(messageToSendBuffer, '\0', sizeof(messageToSendBuffer));

    //Combine the plaintext and key strings and prepare them to be sent over the socket
    concatPlaintextAndKey(plainTextBuffer, keyBuffer, messageToSendBuffer);
//    printf("Concat message is: %s", messageToSendBuffer);



//-------------------------------------------------------
// Establish a socket and connect to the server
//-------------------------------------------------------

    //Create a socket (an endpoint for communication between wo processess), socket() returns a socket descriptor
    client_socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(client_socket_fd < 0){
        printf("Error opening socket\n");
    }

    //Setup address struct for server socket
    //Clears any serverAddress so that we are starting with a clean slate
    bzero((char*) &serverAddress, sizeof(serverAddress));
    //Set network capable address
    serverAddress.sin_family = AF_INET;
    //Store port number
    serverAddress.sin_port = htons(portNumber);

    //Get the DNS entry for this host name
    struct hostent* hostInfo = gethostbyname(hostName);
    if (hostInfo == NULL){
        fprintf(stderr,"CLIENT: ERROR, no such host\n");
        exit(0);
    }
    //Copy first IP address from DNS entry to sin_addr.s_addr
    memcpy((char*)&serverAddress.sin_addr.s_addr,hostInfo->h_addr_list[0],hostInfo->h_length);

    //Connect to server
    if(connect(client_socket_fd, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) < 0){
        fprintf(stderr, "CLIENT: ERROR connecting on port %d\n", portNumber);
        exit(2);
    }

    //Send message to server to confirm connection is coming from enc_client
    char authMsg[5] = "ENC_C";
    int authMsgSent = send(client_socket_fd, &authMsg, 5,0);
    //Check if connection was rejected
    memset(authMsg, '\0', sizeof(authMsg));
    int authMsgRecv = recv(client_socket_fd, &authMsg, 4, 0);
    if(strcmp(authMsg, "REJ") == 0){
        fprintf(stderr, "Connection to dec_server rejected on port %d.\n", portNumber);
        exit(2);
    }


    //Send plaintext + KEY to server
//    sendMessage(client_socket_fd, messageToSendBuffer);

    //Send the server the size of the upcoming message we are going to send
    strSize = htonl(strlen(messageToSendBuffer));
    send(client_socket_fd, &strSize, sizeof(strSize),0);

    //Send plaintext + Key to server
    sendAll(client_socket_fd, messageToSendBuffer);

    // Source: https://linux.die.net/man/3/shutdown  - stop any further send requests
    shutdown(client_socket_fd, SHUT_WR);

    //Get from the server, the length of the cipher its going to send back
    strSize = 0;
    int recvCount = recv(client_socket_fd, &strSize, sizeof(strSize),0);
//    printf("incoming str sizew is: %d\n", ntohl(strSize));

    //Get the cipher from the server
    recvAll(client_socket_fd, cipherFromServerBufer, ntohl(strSize));

    //Output cipher text to stdout
    fflush(stdout);
    fprintf(stdout, "%s", cipherFromServerBufer);
    fflush(stdout);

    close(client_socket_fd);

    return 0;
}

