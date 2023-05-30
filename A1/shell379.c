#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <signal.h>
#include "pcb.h"
#include <time.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include "command.h"
#include "shell_error.h"
#include <fcntl.h>
#include <errno.h>

/* Process table for storing running and suspended processes */
process_table *pcb;

/* Function prototypes */
void process_input();
int count_args(char buf[], int len);
int read_input(char buf[], char buf2[], char * args[], int * num_args);
int is_shell_cmd(char * arg);
int run_shell_cmd(char * args[], int num_args);
int run_cmd(char * args[], int num_args, char * buf);
void time_2_seconds(char *seconds, char *proc_time);
bool check_for_output_file(char * args[], char * output_file, int * num_args);
bool check_for_input_file(char * args[], char * input_file, int * num_args);
void proc_exit();
void shell_sigint();

/* Array of valid shell error strings */
char *err_strings[NUM_SHELL_ERRORS] = {
    // Command Error
    "Command success", 
    "Too many characters provided in an argument",
    "Too many arguments provided",
    "Too many arguments for this command",
    "Process not found",
    "Cannot resume process because it is not suspended",
    "Cannot suspend process because it is not running",
    "Exit command timed out waiting for processes to finish",

    // Process table errors
    "Process table full, could not add process",

    // System call errors
    "System call 'popen' failed",
    "System call 'pclose' failed",
    "System call 'fork' failed",
    "System call 'exec' failed",
    "System call 'wait' failed",
    "System call 'dup2' failed",
    "System call 'close' failed",
    "System call 'dup' failed",
    "System call 'kill' failed"
    };

/* Array of valid shell command strings */
char * shell_cmd_names[] = {"jobs", "exit", "kill", "resume", "suspend", "wait", "sleep"};

/* Function pointer array for shell command callbacks */
int (*shell_cmd_cbs[NUM_SHELL_CMDS])(char * args[], process_table *pcb, bool file_input,
        bool file_output, char * input_file, char * output_file) =
            { jobs_cb, exit_cb, kill_cb, resume_cb, suspend_cb, wait_cb, sleep_cb };

/* Structure array for mapping  with command string with command array indexes
* https://stackoverflow.com/a/16844938
*/
const static struct {
    int val;
    const char *str;
} conversion [] = {
    {0, "jobs"},
    {1, "exit"},
    {2, "kill"},
    {3, "resume"},
    {4, "suspend"},
    {5, "wait"},
    {6, "sleep"}
};


/*
* Function for converting command string to command enum. This is
* used for indexing command callback array using a command string.
* https://stackoverflow.com/a/16844938
*/
int
str2int (const char *str)
{
    int i;
    for(i=0; i<sizeof(conversion)/sizeof(conversion[0]); ++i)
        if (!strcmp (str, conversion[i].str))
            return conversion[i].val;
    return -1;
}


/*
* SHELL379 application
*
* Creates a process table, and continously reads user input to execute commands.
*/
int main() {
    pcb = malloc(sizeof(process_table));
    pcb->num_procs = 0;
    pcb->process_list = NULL;
    // SIGCHLD should be handled to cleanup zombie processes and remove them from process_table
    if (signal(SIGCHLD, proc_exit) == SIG_ERR) {
        perror("Could not create signal handler for SIGCHLD");
    }
    // SIGINT should be handled to kill all child processes
    if (signal(SIGINT, shell_sigint) == SIG_ERR){
        perror("Could not create signal handler for SIGINT");
    }

    char * args[MAX_ARGS+1];
    static char buf[LINE_LENGTH];
    static char buf2[LINE_LENGTH];

    int ret;
    while(1) {
        printf("SHELL379: ");
        static int num_args;
        fflush(stdin);
        fflush(stdout);
        ret = read_input(buf, buf2, args, &num_args);
        if(ret == CMD_OK) {
            if (is_shell_cmd(args[0])) {
                // Run SHELL379 command
                ret = run_shell_cmd(args, num_args);
            } else {
                // Create new process for non SHELL379 command
                ret = run_cmd(args, num_args, buf);
            }
            CHECK_CMD_ERR(ret, err_strings[ret]); 
        } else CHECK_CMD_ERR(ret, err_strings[ret]);
        fflush(stdin);
    }
    return 0;
}


/*
* Executes a SHELL379 command
*/
int run_shell_cmd(char * args[], int num_args) {
    int ret;

    char output_file[MAX_LENGTH];
    bool file_output = check_for_output_file(args, output_file, &num_args);
    char input_file[MAX_LENGTH];
    bool file_input = check_for_input_file(args, input_file, &num_args);

    int cmd = str2int(args[0]);

    ret = shell_cmd_cbs[cmd](args, pcb, file_input, file_output, input_file, output_file);

    return ret;
}


/*
* Executes an arbitrary command
*/
int run_cmd(char * args[], int num_args, char * buf) {
    int ret = CMD_OK;
    char output_file[MAX_LENGTH];
    bool file_output = check_for_output_file(args, output_file, &num_args);
    char input_file[MAX_LENGTH];
    bool file_input = check_for_input_file(args, input_file, &num_args);
    int fd_out;
    int fd_in;
    int pid = fork();
    if( pid == 0 ) {
        // Child process
        bool file_error = false;
        if(file_output) {
            fd_out = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
            if(fd_out == -1) {
                fprintf(stderr, "open failed on output file = %s : %s\n", output_file, strerror(errno));
                file_error = true;
            }
            if(dup2(fd_out, STDOUT_FILENO) == -1) {
                perror("dup2 failed on output file");
                file_error = true;

            }
            if(close(fd_out) == -1) {
                perror("close failed on output file");
                file_error = true;
            }
        }
        if(file_input) {
            fd_in = open(input_file, O_RDWR);
            if(fd_in == -1) {
                fprintf(stderr, "open failed on input file = %s : %s\n", input_file, strerror(errno));
                file_error = true;
            }
            if(dup2(fd_in, STDIN_FILENO) == -1) {
                perror("dup2 failed on input file");
                file_error = true;
            }
            if(close(fd_in) == -1) {
                perror("close failed on input file");
                file_error = true;
            }
        }
        if(file_error) {
            fflush(stdout);
            fflush(stdin);
            _exit(0);
        }
        execvp(args[0], args);
        // execvp will not return if successful
        perror( "Exec problem" );
        fflush(stdout);
        fflush(stdin);
        _exit(0);
    } else if (pid == -1) {
        perror("Fork failed");
        ret = SYS_FORK_FAIL;
    } else {
        // Parent process
        add_process(pcb, pid, buf);
        if(strcmp(args[num_args-1], "&") != 0) {
            if(waitpid(pid, NULL, 0) < 0) {
                fprintf(stderr, "waitpid error on pid = %d : %s\n", pid, strerror(errno));
                ret = SYS_WAIT_FAIL;
            }
        }
    }
    return ret;
}


/*
* Checks if the input command is a SHELL379 command
*/
int is_shell_cmd(char * arg) {
    for(int i=0; i<NUM_SHELL_CMDS; i++) {
        if(strcmp(shell_cmd_names[i], arg) == 0) {
            return true;
        }
    }
    return false;
}


/*
* Read user input 
*/
int read_input(char buf[], char buf2[], char * args[], int * num_args) {
    int i = 0;
    char c;
    bool overflow = false;
    while( (c = getchar())!= '\n') {
        buf2[i++] = c;
        if(i == LINE_LENGTH-1) {
            overflow = true;
            break;
        }
    }
    if (overflow) {
        // Try to get rid of extra input.
        while( (c = getchar())!= '\n') {
        if(i == LINE_LENGTH*10) {
            break;
        }
    }
    }
    buf2[i] = '\0';
    char *arg;
    int size;
    bool cmd_err = false;
    *num_args = count_args(buf2, strlen(buf2));
    strncpy(buf, buf2, strlen(buf2));
    buf[strlen(buf2)] = '\0';
    arg = strtok(buf2, " ");
    for(int i=0; i<MAX_ARGS; i++) {
        // Finished parsing arguments, last argument should be NULL
        if(arg == NULL) {
            args[i] = NULL;
            break;
        }
        size = strlen(arg);
        if(size > MAX_LENGTH) {
            cmd_err = true;
            // Too many characters provided in an argument
            return CMD_MAX_LENGTH_ERR;
        }
        char *s = arg;
        args[i] = s;
        arg = strtok(NULL, " ");
    }
    // Last argument should be NULL
    args[MAX_ARGS] = NULL;
    
    if(!cmd_err && arg != NULL) {
        cmd_err = true;
        // Too many arguments provided
        return CMD_MAX_ARG_ERR;
    }
    return CMD_OK;
}


/*
* Counts the number of arguments in a command
*/
int count_args(char buf[], int len) {
    int count = 1;
    for(int i=0; i<len; i++) {
        if(buf[i] == ' ') {
            count++;
        }
    }
    return count;
}


/*
* Checks if an output file is specified in a command
*/
bool check_for_output_file(char * args[], char * output_file, int * num_args) {
    for(int i=1; i<*num_args; i++) {
        if(args[i][0] == '>') {
            strcpy(output_file, args[i]+1);
            for(int j=i; j<(*num_args); j++) {
                if(j==(*num_args)-1){
                    args[j] = NULL;
                } else {
                    args[j] = args[j+1];
                }
            }
            (*num_args)--;
            return true;
        }
    }
    return false;
}


/*
* Checks if an input file is specified in a command
*/
bool check_for_input_file(char * args[], char * input_file, int * num_args) {
    for(int i=1; i<*num_args; i++) {
        if(args[i][0] == '<') {
            strcpy(input_file, args[i]+1);
            for(int j=i; j<(*num_args); j++) {
                if(j==(*num_args)-1){
                    args[j] = NULL;
                } else {
                    args[j] = args[j+1];
                }
            }
            (*num_args)--;
            return true;
        }
    }
    return false;
}


/*
* Callback function for SIGCHLD handler
*/
void proc_exit() {
    table_cleanup(pcb);
}


/*
* Callback function for SIGINT handler
*/
void shell_sigint() {
    process *proc = pcb->process_list;
    printf("\n");
    for(int i=0; i<pcb->num_procs; i++) {
        if(kill(proc->pid, SIGKILL) < 0) {
            perror("kill signal failed");
        }
    }
    pid_t mypid = getpid();
    if(kill(mypid, SIGKILL) < 0) {
        perror("kill signal failed");
    }
}
