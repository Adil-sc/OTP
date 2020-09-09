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
#include <errno.h>


#define THREAD_POOL_SIZE 5


//Create pool of threads
pthread_t thread_pool[THREAD_POOL_SIZE];
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;


//Create a node struct holding a client socket for a LinkedList
struct node{
    struct node *next;
    int *client_socket;
};

typedef struct node node_t;

node_t *tail = NULL;
node_t *head = NULL;


//Function: enqueue
//--------------------------
//Description: Adds a new node to the back of the LinkedList
//Returns: N/A
void enqueue(int *client_socket_fd){
    //Create a new node and assign in the socket passed in
    node_t *newNode = malloc(sizeof(node_t));
    newNode->client_socket = client_socket_fd;
    newNode->next = NULL;
    //If this is the first node, set the head to point to itself
    if(tail == NULL){
        head = newNode;

    }else{
        tail->next = newNode;
    }
    //Update the tail pointer as the newly added node is now the new tail
    tail = newNode;
}


//Function: dequeue
//--------------------------
//Description: Removes a node from the front of the LinkedList
//Returns: N/A
int *dequeue(){

    //If there is no node to remove, return NULL
    if(head == NULL){
        return NULL;
    }else{
        //Grab the node at the head and return the socket it holds
        int result = head->client_socket;
        node_t *temp = head;
        head = head->next;
        if (head == NULL){
            tail = NULL;
        }
        free(temp);
        return result;
    }
}



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


//Function: generateOneTimePadCipher
//--------------------------
//Description: Takes a string and applys a one time pad operation to it and then sends the cipher
// back to the server
//Returns: N/A
void generateOneTimePadCipher(char *plaintext, char *key, int client_socket_fd){

    char alphabetSet[27] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ ";
    int plaintextLength = strlen(plaintext);
    char cipherMessageBuffer[200000];

    memset(cipherMessageBuffer, '\0', sizeof(cipherMessageBuffer));

    //Implement One-Time-Pad
    for (int i = 0; i < plaintextLength; i++){
//        printf("i is: %d\n", i);

        //Find index position in the alphaSet which corresponds to the current letter in the plaintext
        int plaintextToNumericVal = plaintext[i] - 'A';
        if(plaintext[i] == ' '){
            plaintextToNumericVal = 26;
        }
//        printf("message: %d (%c) \n", plaintextToNumericVal, plaintext[i]);


        //Find index position in the alphaSet which corresponds to the current letter in the key
        int keyToNumericVal = key[i] - 'A';
        if(key[i] == ' '){
            keyToNumericVal = 26;
        }
//        printf("key: %d (%c) \n", keyToNumericVal, key[i]);

        //Combine the plaintext and key index values and add them up
        int messagePlusKey = ((plaintextToNumericVal + keyToNumericVal) % 27);
//        printf(  "Cipher key: %d\n "   , messagePlusKey);

        //Save the encrypted letter value to the cipher buffer
        cipherMessageBuffer[i] = alphabetSet[messagePlusKey];
    }

    //Add newline char to end of cipher before we send it back for grading script compatibility
    cipherMessageBuffer[plaintextLength] = '\n';
//    printf("CIPHER IS: %s", cipherMessageBuffer);

    //Send cipher back to client
    //First let the client know how long the final cipher will be
    int strSize = htonl(strlen(cipherMessageBuffer));
    send(client_socket_fd, &strSize, sizeof(strSize),0);

    sendAll(client_socket_fd, cipherMessageBuffer);
    shutdown(client_socket_fd, SHUT_WR);
}


//Function: getStringToCipher
//--------------------------
//Description: Grabs the string to cipher from the client and performes one time pad on it
//Returns: N/A
void *getStringToCipher(void *p_client_socket_fd){

    int client_socket_fd = *((int*)p_client_socket_fd);
    free(p_client_socket_fd);
    int charsRead;
    char buffer[200000];
    char *plaintext;
    char *key;
    char *tempbuffer;

    memset(buffer, '\0', sizeof(buffer));

    //Read the keygen from client
    //From the client, recv the size of the message which is about to be sent
    //Credit: https://stackoverflow.com/questions/23653753/c-sockets-messages-are-only-sent-once
    int strSize = 0;
    int recvCount = recv(client_socket_fd, &strSize, sizeof(strSize),0);
//    printf("incoming str size is: %d", ntohl(strSize));

    //Read the plaintext + key from the client
    recvAll(client_socket_fd, buffer, ntohl(strSize));

    //Split the received message which is delimited by @@ into separate plaintext and key values
    plaintext = strtok(buffer, "@@");
    key = strtok(NULL ,"@@");

//    printf("Plain text is: %s\n", plaintext);
//    printf("Key is: %s\n", key);

    //Generate the cipher using one time pad
    generateOneTimePadCipher(plaintext, key, client_socket_fd);

    return NULL;
}


//Function: thread_function
//--------------------------
//Description: Function to run when we create a new thread. If there is a new connection socket on the LinkedList
// it will grab the socket and run the getStringToCipher function
// credit: https://www.youtube.com/watch?v=FMNnusHqjpw
//Returns: N/A
void *thread_function(void *arg){

    //Loop for ever and keep checking to see if the LinkedList has a new socket to grab
    while (1){
        int *pclient;
        pthread_mutex_lock(&mutex);
        pclient = dequeue();
        pthread_mutex_unlock(&mutex);
        if(pclient){
            getStringToCipher(pclient);
        }

    }
}


int main(int argc, char *argv[]) {

    //If no port number is provided, notify the user and end the program
    if (argc < 2){
        fprintf(stderr, "No port provided. Terminating program\n", argv[0]);
        exit(1);
    }

    int client_socket_fd, charsRead;
    int portNumber = atoi(argv[1]);
    //Create a socket address. A port number and an IP address help us identify a specific process on a machine
    struct sockaddr_in serverAddress, clientAddress;
    //data type
    socklen_t sizeOfClientInfo = sizeof(clientAddress);

    //Create 5 new threads from the thread pool. This will allow us to have 5 concurrent enc/dec threads
    for(int i =0; i < THREAD_POOL_SIZE; i++){
        pthread_create(&thread_pool[i], NULL, thread_function, NULL);
//        pthread_detach(thread_pool[i]);
    }


    //Create a socket (an endpoint for communication between wo processess), socket() returns a socket descriptor
    int server_socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(server_socket_fd < 0){
        fprintf(stderr, "Error opening socket");
    }

    //Setup address struct for server socket
    //Clears any serverAddress so that we are starting with a clean slate
    bzero((char*) &serverAddress, sizeof(serverAddress));
    //Set network capable address
    serverAddress.sin_family = AF_INET;
    //Store port number
    serverAddress.sin_port = htons(portNumber);
    //Allow client of any address to connect to this server
    serverAddress.sin_addr.s_addr = INADDR_ANY;

    //Bind socket to the port we want
    if(bind(server_socket_fd, (struct sockaddr *)&serverAddress, sizeof(serverAddress)) < 0){
        fprintf(stderr, "Error on binding");
        exit(1);
    }

    //Start listening for connections, allow up to 5 to queue up
    listen(server_socket_fd, 5);

    //Infinite loop which keeps on accepting new connections
    while(1){

        int connectionAuthorized  = 0;
        int authMsgRecv = 0;
        char buffer[5];

        printf("Waiting for a connection on port %d\n", portNumber);
        fflush(stdout);

        //Accept the connection request which creates a connection socket
        //The original server_socket_fd continues to be used to accept new client requests
        client_socket_fd = accept(server_socket_fd, (struct sockaddr*)&clientAddress, &sizeOfClientInfo);
        if (client_socket_fd < 0){
            fprintf(stderr, "Error on accept");
            exit(1);
        }

        // Check to see if the connection is coming from ENC_Client
        // We check to see if we get a "ENC_C" msg from the client. If not, reject the connection
        authMsgRecv = recv(client_socket_fd, &buffer, 5, 0);
        if (strcmp(buffer,"ENC_C") != 0){
            int rejectMsg = send(client_socket_fd, "REJ", 4, 0);
            fprintf(stderr, "ERROR: dec_client cannot use enc_server\n");
        }else{
            //connection is coming from enc_client
            int authMsg = send(client_socket_fd, "OK", 3, 0);
            connectionAuthorized = 1;
        }

        //If connection is authorized, create a new thread
        if(connectionAuthorized){
            printf("SERVER: Connected to client running at host %d, port %d\n", ntohs(clientAddress.sin_addr.s_addr), ntohs(clientAddress.sin_port));

            //Pass in client socket to getStringToCipher
            int *pclient = malloc(sizeof(int));
            *pclient = client_socket_fd;
            //Make sure that if concurrent threads are running, no two threads try to update the queue at the same time
            pthread_mutex_lock(&mutex);
            enqueue(pclient);
            pthread_mutex_unlock(&mutex);
        }

        //DO NOT PUT THIS HERE: https://stackoverflow.com/questions/29400649/linux-socket-bad-file-descriptor
//        close(client_socket_fd);
    }

    close(server_socket_fd);

    return 0;
}

