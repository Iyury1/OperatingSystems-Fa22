#include <sys/wait.h>
#include <stdlib.h>
#include <sys/resource.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "command.h"
#include <errno.h>
#include <fcntl.h>
#include <wait.h>

/* Function prototypes */
static void trim_whtspce(char *arr);
static void deep_sleep(int seconds);


/* 
* Function definition for 'jobs' command 
*/
int jobs_cb(char *args[], process_table *pcb, bool file_input,
        bool file_output, char * input_file, char * output_file) {
    int ret = CMD_OK;
    if(args[1] != NULL) {
        // Is this a background process?
        if(!(strcmp(args[1],"&") == 0) || !(args[2] == NULL)) {
            // Too many arguments for 'jobs' command
            return CMD_ARGS_ERR;
        }
    }
    // Create string to store ps command for run time retrieval
    char s[26];
    int fd_out;
    int saved_stdout;
    if(file_output) {
        fd_out = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
        saved_stdout = dup(1);
        if(saved_stdout == -1) {
            perror("dup2 failed for output file");
            ret = SYS_DUP_FAIL;
        }
        if (dup2(fd_out, STDOUT_FILENO) == -1) {
            perror("dup2 failed for output file");
            ret = SYS_DUP2_FAIL;
        }
        if (close(fd_out) == -1) {
            perror("close failed for output file");
            ret = SYS_CLOSE_FAIL;
        }
    }
    process *proc = pcb->tail;
    printf("\nRunning Processes:\n");
    printf(" #      PID S SEC COMMAND\n");
    // Print status information of each process in process table
    for (int i = 0; i<pcb->num_procs; i++) {
        sprintf(s, "ps --pid %d -o cputimes=", proc->pid);
        // Create string to store run time result of ps command
        char proc_time[10];
        FILE *fd;
        fd = popen(s, "r");
        if(fd == NULL){
            ret = SYS_POPEN_FAIL;
            fprintf(stderr, "popen error on pid = %d : %s\n", proc->pid, strerror(errno));
        }
        fgets(proc_time, 10, fd);
        // Format run time output string
        trim_whtspce(proc_time);
        // Print status information for one process
        printf(" %d: %7d %c %s %s\n", i, proc->pid, proc->status, proc_time, proc->name);
        if(pclose(fd) == -1) {
            ret = SYS_PCLOSE_FAIL;
            fprintf(stderr, "pclose error on pid = %d : %s\n", proc->pid, strerror(errno));
        }
        proc = proc->prev;
    }
    printf("Processes = %5d active\n", pcb->num_procs);
    struct rusage usage;
    getrusage(RUSAGE_CHILDREN, &usage);
    printf("Completed Processes:\n");
    printf("User time = %5d seconds\n", (int) usage.ru_utime.tv_sec);
    printf("Sys  time = %5d seconds\n\n", (int) usage.ru_stime.tv_sec);
    if(file_output) {
        if (dup2(saved_stdout, STDOUT_FILENO) == -1) {
            perror("dup2 failed restoring stdout");
            ret = SYS_DUP2_FAIL;
        }
        if (close(saved_stdout) == -1) {
            perror("close failed for saved stdout");
            ret = SYS_CLOSE_FAIL;
        }
    }
    return ret;
}


/* 
* Function definition for 'exit' command
* 
* If there are any functions caught in infinite loops, exit will hang.
* In the case end program with "Ctrl + c".
*/
int exit_cb(char *args[], process_table *pcb, bool file_input,
        bool file_output, char * input_file, char * output_file) {
    if(args[1] != NULL) {
        if(!(strcmp(args[1],"&") == 0) || !(args[2] == NULL)) {
            // Too many arguments for 'exit' command
            return CMD_ARGS_ERR;
        } 
    }
    process *proc = pcb->process_list;
    int ret = CMD_OK;
    for (int i=0; i<pcb->num_procs; i++) {
        if(waitpid(proc->pid, NULL, 0) < 0) {
            fprintf(stderr, "waitpid error on pid = %d : %s\n", proc->pid, strerror(errno));
            ret = SYS_WAIT_FAIL;
        }
        proc = proc->next;
    }
    exit(0);
    return ret;
}


/*
* Function definition for 'kill' command
*/
int kill_cb(char *args[], process_table *pcb, bool file_input,
        bool file_output, char * input_file, char * output_file) {
    if(args[1] == NULL) {
        // Not enough arguments for 'kill' command
        return CMD_ARGS_ERR;
    } else if (args[2] != NULL) {
        // Is this a background process?
        if(!(strcmp(args[2],"&") == 0) || !(args[3] == NULL)) {
            // Too many arguments for 'kill' command
            return CMD_ARGS_ERR;
        }
    }
    process * proc = find_proc(pcb, atoi(args[1]));
    int ret = CMD_OK;
    if(proc->status == 'S') {
        if(kill(proc->pid, SIGKILL) < 0) {
            perror("kill signal failed");
            return SYS_KILL_FAIL;
        }
        ret = CMD_OK;
    } else{
        char s[MAX_LENGTH];
        sprintf(s, "kill %s", args[1]);
        FILE *fd;
        fd = popen(s, "r");
        if(fd == NULL) {
            perror("popen error on pid");
            ret = SYS_POPEN_FAIL;
        }
        if(pclose(fd) == -1) {
            perror("pclose error");
            ret = SYS_PCLOSE_FAIL;
        }
    }
    
    return ret;
}


/*
* Function definition for 'resume' command 
*/
int resume_cb(char *args[], process_table *pcb, bool file_input,
        bool file_output, char * input_file, char * output_file) {
    if(args[1] == NULL) {
        // Not enough arguments for 'resume' command
        return CMD_ARGS_ERR;
    } else if (args[2] != NULL) {
        // Is this a background process?
        if(!(strcmp(args[2],"&") == 0) || !(args[3] == NULL)) {
            // Too many arguments for 'resume' command
            return CMD_ARGS_ERR;
        }
    }
    process *proc = find_proc(pcb, atoi(args[1]));
    if(proc == NULL) {
        // Process not found
        return CMD_PROC_NOT_FND;
    }
    int ret = CMD_OK;
    // Process is suspended
    if(proc->status == 'S') {
        // Create string to execute kill command to signal process
        // to continue running
        char s[MAX_LENGTH];
        sprintf(s, "kill -s %d %s", SIGCONT, args[1]);
        FILE *fd;
        // Execute kill command
        fd = popen(s, "r");
        if(fd == NULL) {
            perror("popen error on pid");
            ret = SYS_POPEN_FAIL;
        } else {
            // Set status back to 'running'
            proc->status = 'R';
        }
        if(pclose(fd) == -1) {
            perror("pclose error");
            ret = SYS_PCLOSE_FAIL;
        }
    } else {
        // Cannot resume process because it is not suspended
        return CMD_PROC_NOT_SUSP;
    }
    return ret;
}


/*
* Function definition for 'suspend' command
*/
int suspend_cb(char *args[], process_table *pcb, bool file_input,
        bool file_output, char * input_file, char * output_file) {
    if(args[1] == NULL) {
        // Not enough arguments for 'suspend' command
        return CMD_ARGS_ERR;
    } else if (args[2] != NULL) {
        // Is this a background process?
        if(!(strcmp(args[2],"&") == 0) || !(args[3] == NULL)) {
            // Too many arguments for 'suspend' command
            return CMD_ARGS_ERR;
        }
    }
    process *proc = find_proc(pcb, atoi(args[1]));
    if(proc == NULL) {
        // Process not found
        return CMD_PROC_NOT_FND;
    }
    int ret = CMD_OK;
    // Process is running
    if(proc->status == 'R') {
        // Create string to execute kill command to signal process
        // to suspend itself
        char s[MAX_LENGTH];
        sprintf(s, "kill -s %d %s", SIGSTOP, args[1]);
        FILE *fd;
        // Execute kill command
        fd = popen(s, "r");
        if(fd == NULL) {
            perror("popen error on pid");
            ret = SYS_POPEN_FAIL;
        } else {
            // Set status to 'suspended'
            proc->status = 'S';
        }
        if(pclose(fd) == -1) {
            perror("pclose error");
            ret = SYS_PCLOSE_FAIL;
        }
    } else {
        // Cannot suspend process because it is not running
        return CMD_PROC_NOT_RUN;
    }
    return ret;
}


/*
* Function definition for 'wait' command
*/
int wait_cb(char *args[], process_table *pcb, bool file_input,
        bool file_output, char * input_file, char * output_file) {
    if(args[1] == NULL) {
        // Not enough arguments for 'wait' command
        return CMD_ARGS_ERR;
    } else if (args[2] != NULL) {
        // Is this a background process?
        if(!(strcmp(args[2],"&") == 0) || !(args[3] == NULL)) {
            // Too many arguments for 'wait' command
            return CMD_ARGS_ERR;
        }
    }
    // Wait on process with pid = args[1] to return or exit.
    int pid = atoi(args[1]);
    if(waitpid(pid, NULL, 0) < 0) {
        fprintf(stderr, "waitpid error on pid = %d : %s\n", pid, strerror(errno));
        return SYS_WAIT_FAIL;
    }
    return CMD_OK;
}


/*
* Function definition for 'sleep' command
*/
int sleep_cb(char *args[], process_table *pcb, bool file_input,
        bool file_output, char * input_file, char * output_file) {
    if(args[1] == NULL) {
        // Not enough arguments for 'sleep' command
        return CMD_ARGS_ERR;
    } else if (args[2] != NULL) {
        // Is this a background process?
        if(!(strcmp(args[2],"&") == 0) || !(args[3] == NULL)) {
            // Too many arguments for 'sleep' command
            return CMD_ARGS_ERR;
        }
    }
    // Put shell to sleep for duration equal to <args[1]> seconds.
    // Restart sleep if interrupted by signal
    deep_sleep(atoi(args[1]));
    return CMD_OK;
}


/*
* Recursively call sleep until all seconds have elapsed
*/
void deep_sleep(int seconds) {
    int rem;
    if((rem = sleep(seconds)) != 0) {
        deep_sleep(rem);
    }
}


/*
* Removes uneccesary whitespace for cputimes output from ps command.
* This is used so the jobs command output is neat. 
*/
void trim_whtspce(char *arr) {
    int len = strlen(arr);
    int count = len - 1 - 3;
    for(int i=0; i<count; i++){
        arr[i] = arr[i + count];
    }
    arr[3] = '\0';
}