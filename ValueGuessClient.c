#include <stdio.h> 
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netdb.h>
#include "Practical.h"

//used as a buffer for whatever recievefrom() returns, 
//which should only be a single char, but hey, who knows?
#define MAXSTRINGLENGTH 100

int getNextValue(int lastVal, char statusCode);

char checkValue(int val, int sock, struct addrinfo *servAddr);

int main(int argc, char *argv[]){

    if (argc != 3){
        DieWithUserMessage("Parameter(s)", 
            "<Server Address/Name> [<Server Port/ Service>]");
    }

    char *server = argv[1];
    char *servPort = argv[2];

    struct addrinfo addrCriteria;
    memset(&addrCriteria, 0, sizeof(addrCriteria));
    addrCriteria.ai_family = AF_UNSPEC;
    addrCriteria.ai_socktype = SOCK_DGRAM;
    addrCriteria.ai_protocol = IPPROTO_UDP;

    struct addrinfo *servAddr;
    int rtnval = getaddrinfo(server, servPort, &addrCriteria, &servAddr);
    if (rtnval != 0){
        DieWithUserMessage("getaddrinfo() failed", gai_strerror(rtnval));
    }

    int sock = socket(servAddr -> ai_family, servAddr -> ai_socktype, 
        servAddr -> ai_protocol);
    if (sock < 0) {
        DieWithSystemMessage("socket() failed");
    }

    char returnedCode = NULL;
    int nextValue = 0;
    //THIS IS THE LOOP FORMAT SPECIFIED IN THE SPECS AND IT MUST BE USED
    do {
        nextValue = getNextValue(nextValue, returnedCode);

        returnedCode = checkValue(nextValue, sock, servAddr);

        // if returnCode == 0, break and print completion message
        // else, keep looping
    } while (returnedCode != 0);

    //print completion message here

    //CODE BELOW THIS POINT TO BE REORGANIZED TO FIT INSIDE OF THE ABOVE LOOP

    //Must have guess and guesslen initialized by this point
    // currently, they ARE NOWHERE TO BE FOUND!!!!

   
    freeaddrinfo(servAddr);
    close(sock);
    exit(0);
}

// implement a get next val to run in 0(log(n)) time, where n is the range of int values?
int getNextValue(int lastVal, char statusCode){
    int nextVal;

    if (&statusCode == NULL){
        nextVal = 0;
    }
    else if (statusCode == '1'){
        nextVal = lastVal + 1;
    }
    else if (statusCode == '2'){
        nextVal = lastVal - 1;
    }
    else{
        nextVal = 0;
    }

    return nextVal;
}

char checkValue(int val, int sock, struct addrinfo *servAddr){
    int *guess = &val;
    size_t guessLen = sizeof(val);

    // send package with val to server 
    ssize_t numbytes = sendto(sock, guess, guessLen, 0,
        servAddr -> ai_addr, servAddr -> ai_addrlen);
    if (numbytes < 0){
        DieWithSystemMessage("sendto() failed");
    }
    else if (numbytes != guessLen){
        DieWithUserMessage("sendto() error", "sent unexpected number of bytes");
    }
    // get response from server
    struct sockaddr_storage fromAddr;
    socklen_t fromAddrLen = sizeof(fromAddr);

    char returnedCode;
    numbytes = recvfrom(sock, &returnedCode, sizeof(returnedCode), 0, 
        (struct sockaddr*) &fromAddr, &fromAddrLen);
    if (numbytes < 0){
        DieWithSystemMessage("recvfrom() failed");
    }
    else if (numbytes != sizeof(char)){
        DieWithUserMessage("recvfrom() error", "recieved unexpected number of bytes");
    }
    // verify package recived is from server that package was sent to
/*
    below currently commented out, function not yet written
    very unlikely to recive data from unknown server 
    but super important to check before processing revceived packet

    if (!SockAddrsEqual(servAddr -> ai_addr, (struct sockaddr *) &fromAddr)){
        DieWithUserMessage("recvfrom()", "recieved package from unknown source");
    }
*/
    // evaluate validity of response
    if (returnedCode != '0' || returnedCode != '1' || returnedCode != '2'){
        DieWithUserMessage("recvfrom() error", "recieved meaningless value.");
    }

    //return response
    return returnedCode;
}