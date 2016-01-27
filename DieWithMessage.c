#include <stdio.h>
#include <stdlib.h>

void DieWithUserMessage(const char* message, const char* detail){
    fputs(message, stderr);
    fputs(": ", stderr);
    fputs(detail, stderr);
    fputc('\n', stderr);
    exit(1);
}

void DieWithSystemMessage(const char* message){
    perror(message);
    exit(1);
}

