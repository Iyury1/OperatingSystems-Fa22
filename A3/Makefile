CC = gcc
CFLAGS = -O -Wall

all: server client groff

tands.o: tands.h tands.c
	$(CC) $(CFLAGS) -c $^

fifo.o: fifo.h fifo.c
	$(CC) $(CFLAGS) -c $^

server: fifo.o tands.o server.c
	$(CC) $(CFLAGS) -pthread -o $@ $^

client: tands.o client.c
	$(CC) $(CFLAGS) -pthread -o $@ $^

groff: client-server.ms
	groff -ms -T pdf client-server.ms > client-server.pdf
	
clean:
	rm server client *.o
