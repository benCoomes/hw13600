#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <errno.h>
#include <signal.h>
#include <time.h>
#include <arpa/inet.h>
#include "Practical.h"

#define MAXSTRINGLEN 1000
#define MAXVAL 1000000000
//#define VERBOSE

struct clientNode {
    struct sockaddr_in *clientSockaddr;
    struct clientNode *next;
};

int processGuess(int guess, int actual);
int getNewValue();
void sigHandler(int signal);

int messagesSent = 0;
int clientsHandled = 0;
struct clientNode *clientListHead = NULL;

int main(int argc, char *argv[]){
    // parse user arguments
    if(argc != 3 && argc != 5){
        DieWithUserMessage("Parameter(s)", 
            "-p <Server Port/ Service> -v [initial value]");
    }
    in_port_t service = atoi(argv[2]);


    //generate starting value, either user- specified or random
    int theValue;
    if(argc == 5){
        char *valueString = argv[4];
        theValue = (int)(strtol(valueString, NULL, 10));
    }
    else{
        theValue = getNewValue();
    }

    // setup signal handler for SIGINT
    struct sigaction handler;
    sigfillset(&handler.sa_mask);
    handler.sa_flags = 0;
    handler.sa_handler = sigHandler;
    if (sigaction(SIGINT, &handler, 0) < 0){
        fprintf(stderr, "ERROR: sigaction() failed");
        exit(1);
    }

    // fill out sockadd_in structure with information about this server
    struct sockaddr_in servAddrReal;
    struct sockaddr_in *servAddr = &servAddrReal;
    memset(servAddr, 0, sizeof(struct sockaddr_in));
    servAddr->sin_family = AF_INET;
    servAddr->sin_addr.s_addr = htonl(INADDR_ANY);
    servAddr->sin_port = htons(service);

    // build IPV4 UDP socket and bind it to the specified port
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0){
        DieWithSystemMessage("socket() failed");
    }
    if(bind(sock, (struct sockaddr *) servAddr, sizeof(*servAddr)) < 0){
        DieWithSystemMessage("bind() failed");
    }


    // initialize a struct to hold client info
    struct sockaddr_in clntAddr;
    socklen_t clntAddrLen = sizeof(clntAddr);


    // begin main loop, which continues until ^C is entered
    while (true) {
        int recievedValue;
        ssize_t numBytesRecived = recvfrom(sock, &recievedValue, sizeof(recievedValue), 0, 
            (struct sockaddr*) &clntAddr, &clntAddrLen);


        // check to see if client is list of known clients
        uint32_t clientAddress = clntAddr.sin_addr.s_addr;
        in_port_t clientPort = clntAddr.sin_port;
        struct clientNode *nextClient = clientListHead;
        bool clientIsInList = false;
        while (nextClient != NULL && !clientIsInList){
            uint32_t nextClientAddress = nextClient -> clientSockaddr -> 
                sin_addr.s_addr;
            in_port_t nextClientPort = nextClient -> clientSockaddr -> sin_port;

            if(nextClientAddress == clientAddress && nextClientPort == clientPort){
                //client is in list
                clientIsInList = true;
            }
            nextClient = nextClient -> next;
        }
        if (!clientIsInList){
        #ifdef VERBOSE
            printf("New Client! Address and port: %u: %u\n", clientAddress, clientPort);
        #endif
            // add new client to list
            struct clientNode *newClient = malloc(sizeof(struct clientNode));
            newClient -> clientSockaddr = malloc(sizeof(struct sockaddr_in));
            newClient -> clientSockaddr -> sin_addr.s_addr = clientAddress;
            newClient -> clientSockaddr -> sin_port = clientPort;
            newClient -> next = clientListHead;
            clientListHead = newClient;
            clientsHandled++;
        }


        //check clients value and generate the return code
        if (numBytesRecived < 0){
            continue;         
        }
        int returnCode = processGuess(recievedValue, theValue);
        if (returnCode == 0){
            theValue = getNewValue();
        }
  

        // send return code to client
        ssize_t numBytesSent = sendto(sock, &returnCode, sizeof(returnCode), 0,
            (struct sockaddr *) &clntAddr, sizeof(clntAddr)); 
        if (numBytesSent != sizeof(int)){
            DieWithUserMessage("sendto()", "sent unexpected number of bytes");
        }
        messagesSent++;
    }
}


// key: (0, correct) (1, too high) (2, too low)
int processGuess(int guess, int actual){
    int returnCode = 0;
    int difference = guess - actual;
        if (difference > 0){
            returnCode = 1;
        }
        else if (difference < 0){
            returnCode = 2;
        }

    return returnCode;
}

int getNewValue(){
    double randomRatio = ((double)rand()) / RAND_MAX;
    int newVal = (int)(randomRatio * MAXVAL);
    return newVal;
}

void sigHandler(int signalID){
    if(signalID == SIGINT){
        fprintf(stdout, "\n%d\t%d\t", messagesSent, clientsHandled);
        struct clientNode  *nextClient = clientListHead;
        while(nextClient != NULL){
            struct in_addr ipaddr = nextClient -> clientSockaddr -> sin_addr;
            char ipString[100];
            inet_ntop(AF_INET, &ipaddr, ipString, 100); 
            fprintf(stdout, "%s, ", ipString);
            nextClient = nextClient -> next;
        }
        fprintf(stdout, "\n");
    }

    exit(0);
}
