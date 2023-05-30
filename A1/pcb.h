#ifndef _PCB_H_
#define _PCB_H_


#include "shell_error.h"

#define LINE_LENGTH 100 // Max # of characters in an input line
#define MAX_PT_ENTRIES 32 // Max entries in the Process Table

/* Typedef for process entry in process table */
struct process {
    int pid;
    int status;
    char name[LINE_LENGTH];
    struct process *next;
    struct process *prev;
};
typedef struct process process;

/* Typedef for process table to store running and suspended processes */
struct process_table {
    process *process_list;
    process *tail;
    int num_procs;
};
typedef struct process_table process_table;

/* Function prototypes for adding and removing processes to process table */
void remove_process(process_table *proc_table, process *proc);
process *find_proc(process_table *proc_table, int pid);
int add_process(process_table *proc_table, int pid, char *name);
void update_table(process_table *proc_table);

/* Function prototype for waiting on finished processes and removing them from process table */
void table_cleanup(process_table *proc_table);

#endif