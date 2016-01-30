CC=gcc
CFLAGS=-c -Wall -std=c99

all: valueServer valueGuesser

valueServer: DieWithMessage.c ValueGuessServer.c AddressUtility.c
	$(CC) ValueGuessServer.c DieWithMessage.c AddressUtility.c -o valueServer

valueGuesser: DieWithMessage.c ValueGuessClient.c AddressUtility.c
	$(CC) ValueGuessClient.c DieWithMessage.c AddressUtility.c -o valueGuesser

clean: 
	rm client server


