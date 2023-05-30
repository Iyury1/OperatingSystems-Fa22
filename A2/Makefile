CC = gcc
CFLAGS = -O -Wall
DCFLAGS = -g -Wall

all: prodcon

debug: d_prodcon

fifo.o: fifo.h fifo.c
	$(CC) $(CFLAGS) -c fifo.c

tands.o: tands.h tands.c
	$(CC) $(CFLAGS) -c tands.c

prodcon: tands.o fifo.o prodcon.c
	$(CC) $(CFLAGS) -pthread -o prodcon prodcon.c tands.o fifo.o

d_fifo.o: fifo.h fifo.c
	$(CC) $(DCFLAGS) -c fifo.c -o d_fifo.o

d_tands.o: tands.h tands.c
	$(CC) $(DCFLAGS) -c tands.c -o d_tands.o

d_prodcon: d_tands.o d_fifo.o prodcon.c
	$(CC) $(DCFLAGS) -pthread -o prodcon prodcon.c d_tands.o d_fifo.o

clean:
	rm *.o
	rm prodcon
