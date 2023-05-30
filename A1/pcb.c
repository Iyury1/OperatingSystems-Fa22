#include "pcb.h"
#include <stdlib.h>
#include <stdio.h>
#include <sys/wait.h>
#include <errno.h>
#include <string.h>
#include "command.h"

/*
* Removes processes from process table and reassigns links to next and previous entries
*/
void remove_process(process_table *proc_table, process *proc) {
    if(proc_table->process_list != NULL) {
        if(proc_table->process_list != proc) {
            // Reassign next pointers to link list forwards
            proc->prev->next = proc->next;
            // Reassign prev pointers to link list backwards
            if(proc->next != NULL) {
                proc->next->prev = proc->prev;
            } else {
                proc_table->tail = proc->prev;
            }
            proc->next = NULL;
            proc->prev = NULL;
        } else {
            // Process is at the head of the list, set the next process
            // to be the new head
            proc_table->process_list = proc->next;
            if(proc_table->process_list != NULL) {
                // The list had more than one process
                proc_table->process_list->prev = NULL;
            } else {
                proc_table->tail = NULL;
            }
        }
        proc_table->num_procs--;
        free(proc);
    }
}


/*
* Retrieve a process from a process table
*/
process *find_proc(process_table *proc_table, int pid) {
    if(proc_table->process_list != NULL) {
        process *temp = proc_table->process_list;
        while(temp != NULL) {
            if(temp->pid == pid) {
                return temp;
            }
            temp = temp->next;
        }
    }
    return NULL;
}


/* 
* Adds process to the process table if the table is not already full
*/
int add_process(process_table *proc_table, int pid, char *name) {
    if(proc_table->num_procs < MAX_PT_ENTRIES) {
        process *proc = malloc(sizeof(process));
        proc->pid = pid;
        proc->status = 'R';
        strcpy(proc->name, name);
        // Insert process to head of process list
        proc->next = proc_table->process_list;
        if(proc_table->process_list != NULL) {
            (proc_table->process_list )->prev = proc;
        } else {
            proc_table->tail = proc;
        }
        proc_table->process_list = proc;
        proc->prev = NULL;
        proc_table->num_procs++;
        return PROC_OK;
    }

    return PROC_TABLE_FULL;
}


/*
* Debugging artifact
*/
void printlist(process *head) {
    process *temp = head;
    while (temp != NULL) {
        printf("%d - ", temp->pid);
        temp = temp->next;
    }
}

/* 
* Function defintion for waiting on finished processes and removing them from process table
* 
* Checks every entry in the process table and calls waitpid() on that process. The fucntion will not
* will not block on waitpid() because WNOHANG was specified. The process will be removed from the table
* if the process has returned or exited. This prevents zombie processes from being created, and also
* frees the process table of completed processes.
*/
void table_cleanup(process_table *proc_table) {
    int pid;
    process *proc = proc_table->process_list;
    int status;
    for (int i=0; i<proc_table->num_procs; i++) {
        if ((pid = waitpid(proc->pid, &status, WNOHANG)) > 0) {
            remove_process(proc_table, proc);
        }
        if (pid < 0) {
            // Ignore errno for no child process -- process was already waited for
            if(errno == 10) {
                remove_process(proc_table, proc);
            } else {
                fprintf(stderr, "waitpid error on pid = %d : %s\n", proc->pid, strerror(errno));
            }
        }
        proc = proc->next;
    }
}
