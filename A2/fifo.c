#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <limits.h>

#include "fifo.h"

#define FIFO_EMPTY INT_MIN


void fifo_init(fifo *f, int size) {
    f->size = size;
    f->entries = malloc(sizeof(int) * f->size);
    f->count = 0;
    f->head = 0;
    f->tail = 0;
}

bool fifo_empty(fifo *f) {
    return(f->count == 0);
}

bool fifo_full(fifo *f) {
    return(f->count == f->size);
}

void fifo_deinit(fifo *f) {
    free(f->entries);
}

bool enqueue(int entry, fifo *f) {
    if(fifo_full(f)) return false;
    f->entries[f->tail] = entry;
    f->count++;
    f->tail = (f->tail + 1) % f->size;

    return true;
}

int dequeue(fifo *f) {
    if(fifo_empty(f)) return FIFO_EMPTY;
    int res = f->entries[f->head];
    f->head = (f->head + 1) % f->size;
    f->count--;
    return res;
}
