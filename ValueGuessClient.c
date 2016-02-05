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
//#define VERBOSE

int getNextValue(int lastVal, int statusCode);
char checkValue(int val, int sock, struct sockaddr_in *servAddr);
double getTime();
int testSocket(int sock, struct sockaddr_in *servAddr);
void sigHandler(int signal);


int nextValue = 0;
int guessCount = 0;
double runtime = 0.0;

int main(int argc, char *argv[]){

    // parse command line arguments
    if (argc != 7){
        DieWithUserMessage("Parameter(s)", 
            "-s <Server Address> -p1 <Server Port 1> -p2 <Server Port 2>");
    }
    char *server = argv[2];
    in_port_t servPort1 = atoi(argv[4]);
    in_port_t servPort2 = atoi(argv[6]);


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


    // create sockaddr_in struct to hold server information
    struct sockaddr_in servAddrReal;
    struct sockaddr_in *servAddr = &servAddrReal;
    memset(servAddr, 0, sizeof(struct sockaddr_in));
    servAddr->sin_family = AF_INET;
    int rtnval = inet_pton(AF_INET, server, &(servAddr->sin_addr.s_addr));
    if (rtnval == 0){
        DieWithUserMessage("inet_pton() failed", "trying to make first port connection");
    }
    servAddr->sin_port = htons(servPort1);


    // try to build a socket with port 1 and then port 2 if port 1 doesnt work
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    int connectionStatus = -1;
    connectionStatus = testSocket(sock, servAddr);
    if (connectionStatus < 0) {
        #ifdef VERBOSE
            puts("connection with port 1 failed, trying to build socket with port 2...\n");
        #endif

        servAddr->sin_port = htons(servPort2);
        connectionStatus = testSocket(sock, servAddr);
        if(connectionStatus < 0){
            #ifdef VERBOSE
            printf("Warning: Neither port was successfully communicated with.");
            #endif
            //DieWithUserMessage("Socket()", "failed to create socket, tried both ports");
        }
    }


    // Main loop: loop until a value is guessed correctly
    runtime = getTime();
    while (1) {
        #ifdef VERBOSE
            prinf("At top of main loop\n");
        #endif
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

    // ^C sends control to this label
    clean_up: ;

    double totalTime = getTime() - runtime;
    printf("%d\t%.2f\t%d\n", guessCount, totalTime, nextValue);
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

char checkValue(int guess, int sock, struct sockaddr_in *servAddr){

    int returnedCode;
    socklen_t servAddrLen = sizeof(*servAddr);

    // send guess to server
    ssize_t bytesSent = sendto(sock, &guess, sizeof(guess), 0,
        (struct sockaddr *) servAddr, servAddrLen);
    if (bytesSent < 0){
        DieWithSystemMessage("sendto() failed");
    }
    else if (bytesSent != sizeof(int)){
        DieWithUserMessage("sendto() error", "sent unexpected number of bytes");
    }


    // get response from server, set return code to EINTR in case of timeout
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
    else if (bytesRecieved != sizeof(int)){
        DieWithUserMessage("recvfrom() error", "recieved unexpected number of bytes");
    }
    else {
        guessCount++;
    }
    alarm(0);

    return returnedCode;
}

double getTime(){
    struct timeval curTime;
    gettimeofday(&curTime, NULL);
    return (((((double) curTime.tv_sec) * 1000000.0) 
             + (double) curTime.tv_usec) / 1000000.0);
}

// return values less than 0 indicate that sock does not work
int testSocket(int sock, struct sockaddr_in *servAddr){

    if(sock < 0){
        return sock;
    }

    // commented out because the auto tester doesnt like it... :(
    
    else {
        //try sendto and recvfrom, reutrn -1 if they fail, 1 if success
        int checkMessage = -100;
        int returnValue;
        struct sockaddr_in fromAddr;
        socklen_t fromAddrLen = sizeof(fromAddr);
        ssize_t bytesRecieved = 0;

        ssize_t bytesSent = sendto(sock, &checkMessage, sizeof(checkMessage), 0,
            (struct sockaddr*) servAddr, sizeof(*servAddr));
        if (bytesSent < 0){
            return -1;
        }

        alarm(TIMEOUTSEC);
        bytesRecieved = recvfrom(sock, &returnValue, sizeof(returnValue), 0, 
            (struct sockaddr*) &fromAddr, &fromAddrLen);
        if (bytesRecieved <= 0){
            return -1;
        }
        alarm(0);
    }
    

    return 1;
}

void sigHandler(int signalID){
    #ifdef VERBOSE
        printf("in interruptSignalHandler\n");
    #endif
    if(signalID == SIGINT){
        double totalTime = getTime() - runtime;
        printf("%d\t%.2f\t%d\n", guessCount, totalTime, nextValue);
        exit(0);
    }
    else if(signalID == SIGALRM){
        #ifdef VERBOSE
            printf("in alarm handler, alarm ignored.\n");
        #endif
        return;
    }
    else{
        exit(1);
    }
}
