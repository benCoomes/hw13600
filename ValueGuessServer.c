#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include "Practical.h"

#define MAXSTRINGLEN 1000

int main(int argc, char *argv[]){

    if(argc != 2){
        DieWithUserMessage("Parameter(s)", "<Server Port/ Service>");
    }

    char *service = argv[1];
    struct addrinfo addrCriteria;
    memset(&addrCriteria, 0, sizeof(addrCriteria));
    addrCriteria.ai_family = AF_UNSPEC;
    addrCriteria.ai_flags = AI_PASSIVE;
    addrCriteria.ai_socktype = SOCK_DGRAM;
    addrCriteria.ai_protocol = IPPROTO_UDP;


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
    struct addrinfo *servAddr;
    int rtnVal = getaddrinfo(NULL, service, &addrCriteria, &servAddr);
    if (rtnVal != 0){
        // handle getaddrinfo failure
        DieWithUserMessage("getaddrinfo() failed", gai_strerror(rtnVal));
    }

    int sock = socket(servAddr->ai_family, servAddr->ai_socktype, servAddr->ai_protocol);
    if (sock < 0){
        // handle socket creation error
        DieWithSystemMessage("socket() failed");
    }

    if(bind(sock, servAddr->ai_addr, servAddr->ai_addrlen) < 0){
        // handle binding failure
        DieWithSystemMessage("bind() failed");
    }

    /* 
    void freeaddrinfo(struct addrinfo *addrlist)
    this is used to free a linked list of addrinfo structs

    Q: why is the pointer to the head of the list not passed? It seems odd that 
        getaddrinfo uses a pointer to the first node and freeaddrinfo just uses
        the address of the first node
    A: ???
    */
    freeaddrinfo(servAddr);

    while(true){
        struct sockaddr_storage clntAddr;
        socklen_t clntAddrLen = sizeof(clntAddr);

        // create buffer for messages and then block until one arrives
        char buffer[MAXSTRINGLEN];
        ssize_t numBytesRecived = recvfrom(sock, buffer, MAXSTRINGLEN, 0, 
            (struct sockaddr*) &clntAddr, &clntAddrLen);
        if(numBytesRecived < 0){
            // handle recvfrom() failure
            DieWithSystemMessage("recvfrom() failed");
        }

        fputs("Handling Client ", stdout);
        PrintSocketAddress((struct sockaddr *) &clntAddr, stdout);
        fputc('\n', stdout);

        ssize_t numBytesSent = sendto(sock, buffer, numBytesRecived, 0,
            (struct sockaddr *) &clntAddr, sizeof(clntAddr)); //why not use clntaddrlen as in line 45? try changing and see what happens
        if (numBytesSent < 0){
            // handle sendto() failure
            DieWithSystemMessage("sendto() failed");
        }
        else if(numBytesSent != numBytesRecived){
            //handle unexpected number of bytes sent
            DieWithUserMessage("sendto()", "sent unexpected number of bytes");
        }
    }
}