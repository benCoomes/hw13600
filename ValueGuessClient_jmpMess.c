#include <stdio.h> 
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netdb.h>
#include <signal.h>
#include <time.h>
#include <setjmp.h>
#include "Practical.h"

int getNextValue(int lastVal, int statusCode);

char checkValue(int val, int sock, struct addrinfo *servAddr);

double getTime();

void interruptSignalHandler(int signal);
struct sigaction handler;


jmp_buf jumpState;

int main(int argc, char *argv[]){

    int prematureJump = setjmp(jumpState);


    memset(&handler,0,sizeof(handler));
    handler.sa_handler = interruptSignalHandler;
    if(sigaction(SIGINT, &handler, 0) < 0){
        fprintf(stderr, "ERROR: sigaction() failed");
        exit(1);
    }
    if(sigaction(SIGALRM, &handler, 0) < 0){
        fprintf(stderr, "ERROR: sigaction() failed");
        exit(1);
    }

    if (prematureJump != 0) {
        printf("User interupt, essential variables not yet initialized");
    }

    // parse command line arguments
    if (argc != 4){
        DieWithUserMessage("Parameter(s)", 
            "<Server Address> <Server Port 1> <Server Port 2>");
    }
    char *server = argv[1];
    char *servPort1 = argv[2];
    char *servPort2 = argv[3];

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

    // try to build a socket with port 1 and then port 2

    /*my notes: it seems that sockets can still be built even if the port is closed
                of course, nothing happens when an incorrect port is used.
                How can a closed port be detected? should I implement a handshake
                routine and use a timeout to indicate that the client/server connection
                is not working?
    */
    int sock = socket(servAddr-> ai_family, servAddr-> ai_socktype, 
        servAddr-> ai_protocol);
    if (sock < 0) {
        puts("Port 1 failed, trying to build socket with port 2...\n");

        freeaddrinfo(servAddr); // Is servAddr usable in getaddrinfo after this?
        int rtnval = getaddrinfo(server, servPort2, &addrCriteria, &servAddr);
        if (rtnval != 0){
            DieWithUserMessage("getaddrinfo() failed", gai_strerror(rtnval));
        }

        sock = socket(servAddr-> ai_family, servAddr-> ai_socktype, 
            servAddr-> ai_protocol);
        if(sock < 0){
            DieWithUserMessage("Socket()", "failed to create socket, tried both ports");
        }
    }


    // Main loop: loop until a value is guessed correctly
    // 4 is arbitrary, only 0, 1, and 2 are meaningful returnedCode values
    int returnedCode = 4;
    int nextValue = 0;
    int guessCount = 0;
    double runtime = getTime();
    do {
        printf("at top of main loop, ");
        int jumpCode = setjmp(jumpState);
        printf("jumpcode is: %d\n", jumpCode);
        switch(jumpCode){
            case SIGALRM:
                // case for when alarm goes off!
                printf("alarm went off!\n");
                break;
            case SIGINT: 
                //case for when user interrupt is called!
                goto clean_up;
                break; // unneccesay, but makes me feel better
            case 0:
                //case for when normal execution should occur
                printf("in case 0 of main loop\n");
                alarm(1);
                nextValue = getNextValue(nextValue, returnedCode);
                returnedCode = checkValue(nextValue, sock, servAddr);
                guessCount++;
                alarm(0);
                break;
            default:
                //long jump called, fatal error!
                exit(1);
        }
       
    } while (returnedCode != 0);



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

    int returnedCode;
    ssize_t bytesRecieved = recvfrom(sock, &returnedCode, sizeof(returnedCode), 0, 
        (struct sockaddr*) &fromAddr, &fromAddrLen);
    if (bytesRecieved < 0){
        DieWithSystemMessage("recvfrom() failed");
    }
    else if (bytesRecieved != sizeof(int)){
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
    if (returnedCode != 0 && returnedCode != 1 && returnedCode != 2){
        DieWithUserMessage("recvfrom() error", "recieved meaningless value.");
    }

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

void interruptSignalHandler(int signalID){
    printf("in interruptSignalHandler\n");
    if(signalID == SIGINT){
        // jump with code indicating that a user interupt occured
        printf("taking long jump to sigint case\n");
        longjmp(jumpState, signalID);
        return;
    }
    else if(signalID == SIGALRM){
        // jump with code indicating that an alarm popped
        printf("taking long jump to sigalrm case\n");
        alarm(1);
        longjmp(jumpState, signalID);
        return;
    }
    else{
        exit(1);
    }
}
