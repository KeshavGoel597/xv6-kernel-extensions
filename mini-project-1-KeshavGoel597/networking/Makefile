
# S.H.A.M. Reliable UDP
CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -O2 -g
LDFLAGS = -lssl -lcrypto

.PHONY: all clean

all: client server

client: client.c sham.h
	$(CC) $(CFLAGS) -o client client.c $(LDFLAGS)

server: server.c sham.h  
	$(CC) $(CFLAGS) -o server server.c $(LDFLAGS)

clean:
	rm -f client server *.o
