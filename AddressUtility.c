#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>


//see page 42 in sockets book for details and context
void PrintSocketAddress(const struct sockaddr *address, FILE *stream){
    //Test for addresses and stream
    if (address == NULL || stream == NULL)
        return;

    void *numericAddress; // pointer to binary address
    char addrBuffer[INET6_ADDRSTRLEN];
    in_port_t port;
    switch (address->sa_family) {
    case AF_INET:
        // cast generic sockaddr type to IPV4-specific sockaddr_in type,
        //then get the address of the value of address->sin_addr
        // (-> operator preceeds & operator)
        numericAddress = &((struct sockaddr_in *) address)->sin_addr;
        port = ntohs(((struct sockaddr_in *) address)->sin_port) ;
        break;
    case AF_INET6:
        numericAddress = &((struct sockaddr_in6 *) address)->sin6_addr;
        port = ntohs(((struct sockaddr_in6 *) address)->sin6_port) ;
        break;
    default:
        fputs("[unknown type]", stream); // unhandled type
        return;
    }

    if (inet_ntop(address->sa_family, numericAddress, addrBuffer, 
            sizeof addrBuffer) == NULL)
        fputs("[invalid address", stream); // unable to convert

    else {
        fprintf(stream, "%s", addrBuffer);
        if (port != 0){
            fprintf(stream, "-%u", port);
        }
    }
}