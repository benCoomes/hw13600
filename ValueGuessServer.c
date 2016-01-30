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
    if(argc < 2 || argc > 3){
        DieWithUserMessage("Parameter(s)", 
            "<Server Port/ Service> [initial value]");
    }
    in_port_t service = atoi(argv[1]);


    // setup signal handler for SIGINT
    struct sigaction handler;
    sigfillset(&handler.sa_mask);
    handler.sa_flags = 0;
    handler.sa_handler = sigHandler;
    if (sigaction(SIGINT, &handler, 0) < 0){
        fprintf(stderr, "ERROR: sigaction() failed");
        exit(1);
    }

    // generate addrinfo linked list
/*
    struct addrinfo addrCriteria;
    memset(&addrCriteria, 0, sizeof(addrCriteria));
    addrCriteria.ai_family = AF_UNSPEC;
    addrCriteria.ai_flags = AI_PASSIVE;
    addrCriteria.ai_socktype = SOCK_DGRAM;
    addrCriteria.ai_protocol = IPPROTO_UDP;
    struct addrinfo *servAddr;
    int rtnVal = getaddrinfo(NULL, service, &addrCriteria, &servAddr);
    if (rtnVal != 0){
        // handle getaddrinfo failure
        DieWithUserMessage("getaddrinfo() failed", gai_strerror(rtnVal));
    }
**/
    struct sockaddr_in servAddrReal;
    struct sockaddr_in *servAddr = &servAddrReal;
    memset(servAddr, 0, sizeof(struct sockaddr_in));
    servAddr->sin_family = AF_INET;
    servAddr->sin_addr.s_addr = htonl(INADDR_ANY);
    servAddr->sin_port = htons(service);

    // build socket with servAddr linked list
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0){
        DieWithSystemMessage("socket() failed");
    }
    if(bind(sock, (struct sockaddr *) servAddr, sizeof(*servAddr)) < 0){
        // handle binding failure
        DieWithSystemMessage("bind() failed");
    }

    // initialize a struct to hold client info
    struct sockaddr_in clntAddr;
    socklen_t clntAddrLen = sizeof(clntAddr);

    //generate starting value, either user- specified or random
    int theValue;
    if(argc == 3){
        char *valueString = argv[2];
        theValue = (int)(strtol(valueString, NULL, 10));
    }
    else{
        theValue = getNewValue();
    }

    while (true) {
        // recieve value (unknown client!)
        int recievedValue;
        ssize_t numBytesRecived = recvfrom(sock, &recievedValue, sizeof(recievedValue), 0, 
            (struct sockaddr*) &clntAddr, &clntAddrLen);
        //could be point of failure, if server processes a metric shit ton of messages

        // check to see if client is in client list
        //Yes: use that client
        //No: create new client object to store data about the new client
// manage client list
        uint32_t clientAddress = clntAddr.sin_addr.s_addr;
        in_port_t clientPort = clntAddr.sin_port;

        struct clientNode *nextClient = clientListHead;
        bool clientIsInList = false;
        while (nextClient != NULL && !clientIsInList){
            // see pg 24 to understand this nonsense
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
// end manage client list


        //check clients value and generate the return code
        if (numBytesRecived < 0){
            // perhaps just return incorrect guess code instead of failing??
            printf("bytes recieved: %d", (int)numBytesRecived);
            DieWithSystemMessage("recvfrom() failed");           
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



    /*
    struct addrinfo {
        int ai_flags;               // Flags to control info resolution
        int ai_family;              // Family: AF_INET, AF_INET6, AF_UNSPEC
        int ai_socktype;            // Socket Type: SOCK_STREAM || SOCK_DGRAM
        int ai_protocol;            // Protocol (0 is default) IPPROTO_XXX
        socklen_t ai_addrlen;       // length of sock address for ai_addr
        struct sockaddr *ai_addr;   // socket address for socket
        char *ai_cannoname;         // Cannonical name
        struct addrinfo *ai_next;   // next addrinfo in linked list
    }


    getaddrinfo(const char *hostString, const char *serviceString, 
        const struct addrinfo *hints, struct addrinfo **results)

    hostString is a char string that represents a host name
    serviceString is a char string that represents a service name
    hints is a addrinfo struct used to filter the results
    results is a pointer to the first element in a linked list of 
        addrinfo structures

    Q: why pass a pointer to a pointer for last parameter?
    A: servAddr returns a linked list, and servAddr is the first data node in 
        the list, so a head pointer must be passsed, and this head pointer is 
        the pointer to servAddr
    */