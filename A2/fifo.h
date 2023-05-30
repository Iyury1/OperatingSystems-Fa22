#ifndef __FIFO_H__
#define __FIFO_H__

typedef struct {
    int *entries;
    int head, tail, count, size;
} fifo;

void fifo_init(fifo *f, int size);

bool fifo_empty(fifo *f);

bool fifo_full(fifo *f);

void fifo_deinit(fifo *f);

bool enqueue(int entry, fifo *f);

int dequeue(fifo *f);

#endif