CC=gcc
CFLAGS=-c -Wall -std=c99

all: server client

server: DieWithMessage.c ValueGuessServer.c AddressUtility.c
	$(CC) ValueGuessServer.c DieWithMessage.c AddressUtility.c -oserver

client: DieWithMessage.c ValueGuessClient.c AddressUtility.c
	$(CC) ValueGuessClient.c DieWithMessage.c AddressUtility.c -oclient

clean: 
	rm client server


