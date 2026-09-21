CC=gcc
CFLAGS=-Wall -Werror
#LDFLAGS=-lpthread

all: client server

client: client.c
	$(CC) $(CFLAGS) client.c -o client

server: server.c
	$(CC) $(CFLAGS) server.c -o server

clean:
	rm -f client server