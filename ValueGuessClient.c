#include <stdio.h> 
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netdb.h>
#include <signal.h>
#include <time.h>
#include <errno.h>
#include "Practical.h"

#define TIMEOUTSEC 1

int getNextValue(int lastVal, int statusCode);
char checkValue(int val, int sock, struct addrinfo *servAddr);
double getTime();
int testSocket(int sock, struct addrinfo *servAddr);
void sigHandler(int signal);


int nextValue = 0;
int guessCount = 0;
double runtime = 0.0;

int main(int argc, char *argv[]){

    // parse command line arguments
    if (argc != 4){
        DieWithUserMessage("Parameter(s)", 
            "<Server Address> <Server Port 1> <Server Port 2>");
    }
    char *server = argv[1];
    char *servPort1 = argv[2];
    char *servPort2 = argv[3];


    // setup signal handler for SIGALRM and SIGINT
    struct sigaction handler;
    sigfillset(&handler.sa_mask);
    handler.sa_flags = 0;
    handler.sa_handler = sigHandler;
    if(sigaction(SIGINT, &handler, 0) < 0){
        fprintf(stderr, "ERROR: sigaction() failed");
        exit(1);
    }
    if(sigaction(SIGALRM, &handler, 0) < 0){
        fprintf(stderr, "ERROR: sigaction() failed");
        exit(1);
    }


    // generate addrinfo linked list using the first port option
    struct addrinfo addrCriteria;
    memset(&addrCriteria, 0, sizeof(addrCriteria));
    addrCriteria.ai_family = AF_UNSPEC;
    addrCriteria.ai_socktype = SOCK_DGRAM;
    addrCriteria.ai_protocol = IPPROTO_UDP;
    struct addrinfo *servAddr;
    int rtnval = getaddrinfo(server, servPort1, &addrCriteria, &servAddr);
    if (rtnval != 0){
        DieWithUserMessage("getaddrinfo() failed", gai_strerror(rtnval));
    }

    // try to build a socket with port 1 and then port 2, and test if it works
    int sock = socket(servAddr-> ai_family, servAddr-> ai_socktype, 
        servAddr-> ai_protocol);
    int socketStatus = -1;
    socketStatus = testSocket(sock, servAddr);
    if (socketStatus < 0) {
        puts("sock() with port 1 failed, trying to build socket with port 2...\n");

        freeaddrinfo(servAddr); // Is servAddr usable in getaddrinfo after this?
        int rtnval = getaddrinfo(server, servPort2, &addrCriteria, &servAddr);
        if (rtnval != 0){
            DieWithUserMessage("getaddrinfo() failed", gai_strerror(rtnval));
        }

        sock = socket(servAddr-> ai_family, servAddr-> ai_socktype, 
            servAddr-> ai_protocol);
        socketStatus = testSocket(sock, servAddr);
        if(socketStatus < 0){
            DieWithUserMessage("Socket()", "failed to create socket, tried both ports");
        }
    }


    // Main loop: loop until a value is guessed correctly
    runtime = getTime();
    while (1) {
        int returnCode = checkValue(nextValue, sock, servAddr);
        switch(returnCode){
            case 1:
            case 2:
                //for guesses that were incorrecct
                nextValue = getNextValue(nextValue, returnCode);
                break;

            case EINTR:
                // for cases where recvfrom timeout'd, continue with loop
                break;

            case 0:
                // for guesses that were correct
                goto clean_up;
                break;
        }
       
    }


    clean_up: ;

    double totalTime = getTime() - runtime;
    //print completion message: 
    //number of guesses, running time, correctly guessed value
    printf("%d\t%.2f\t%d\n", guessCount, totalTime, nextValue);
    freeaddrinfo(servAddr);
    close(sock);
    exit(0);
}



int getNextValue(int lastVal, int statusCode){
    int nextVal;

    // last guess was too high
    if (statusCode == 1){
        nextVal = lastVal - 1;
    }
    // last guess was too small
    else if (statusCode == 2){
        nextVal = lastVal + 31622;
    }
    else{
        nextVal = lastVal;
    }
    
    return nextVal;
}

char checkValue(int guess, int sock, struct addrinfo *servAddr){

    int returnedCode;

    // send package with val to server 
    ssize_t bytesSent = sendto(sock, &guess, sizeof(guess), 0,
        servAddr -> ai_addr, servAddr -> ai_addrlen);
    if (bytesSent < 0){
        DieWithSystemMessage("sendto() failed");
    }
    else if (bytesSent != sizeof(int)){
        DieWithUserMessage("sendto() error", "sent unexpected number of bytes");
    }
    // get response from server
    struct sockaddr_storage fromAddr;
    socklen_t fromAddrLen = sizeof(fromAddr);

    alarm(TIMEOUTSEC);
    ssize_t bytesRecieved = recvfrom(sock, &returnedCode, sizeof(returnedCode), 0, 
        (struct sockaddr*) &fromAddr, &fromAddrLen);
    if (bytesRecieved < 0){
        if (errno = EINTR){
            // alarm went off
            returnedCode = EINTR;
        }
        else{
            DieWithSystemMessage("recvfrom() failed");
        }
    }
    // DOES THIS AFFECT ALARMS???? NO - else if
    else if (bytesRecieved != sizeof(int)){
        DieWithUserMessage("recvfrom() error", "recieved unexpected number of bytes");
    }
    else {
        guessCount++;
    }
    alarm(0);
    
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
    /*if (returnedCode != 0 && returnedCode != 1 && returnedCode != 2){
        DieWithUserMessage("recvfrom() error", "recieved meaningless value.");
    }*/

    //return response
    printf("returnedCode: %d\n", returnedCode);
    return returnedCode;
}

double getTime(){
    struct timeval curTime;
    gettimeofday(&curTime, NULL);
    return (((((double) curTime.tv_sec) * 1000000.0) 
             + (double) curTime.tv_usec) / 1000000.0);
}

// return values less than 0 indicate that sock does not work
int testSocket(int sock, struct addrinfo *servAddr){
    if(sock < 0){
        return sock;
    }
    else {
        //try sendto and recvfrom, reutrn -1 if they fail, 1 if success
        int checkMessage = -100;
        int returnValue;
        struct sockaddr_storage fromAddr;
        socklen_t fromAddrLen = sizeof(fromAddr);
        ssize_t bytesRecieved;

        ssize_t bytesSent = sendto(sock, &checkMessage, sizeof(checkMessage), 0,
            servAddr -> ai_addr, servAddr -> ai_addrlen);
        if (bytesSent < 0){
            return -1;
        }

        alarm(TIMEOUTSEC);
        bytesRecieved = recvfrom(sock, &returnValue, sizeof(returnValue), 0, 
            (struct sockaddr*) &fromAddr, &fromAddrLen);
        if (bytesRecieved < 0){
            return -1;
        }
        alarm(0);
    }

    return 1;
}

void sigHandler(int signalID){
    printf("in interruptSignalHandler\n");
    if(signalID == SIGINT){
        double totalTime = getTime() - runtime;
        printf("%d\t%.2f\t%d\n", guessCount, totalTime, nextValue);
        exit(0);
    }
    else if(signalID == SIGALRM){
        printf("in alarm handler, alarm ignored.\n");
        return;
    }
    else{
        exit(1);
    }
}
