#ifndef __FIFO_H__
#define __FIFO_H__

#include <stdbool.h>

typedef struct transaction {
    int count;
    int client_fd;
    int work;
} transaction;

typedef struct {
    transaction *entries;
    int head, tail, count, size;
} fifo;

void fifo_init(fifo *f, int size);

bool fifo_empty(fifo *f);

bool fifo_full(fifo *f);

void fifo_deinit(fifo *f);

bool enqueue(transaction entry, fifo *f);

bool dequeue(transaction *t, fifo *f);

#endif