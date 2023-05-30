#ifndef __COMMAND_H_
#define __COMMAND_H__

#include "pcb.h"
#include "shell_error.h"
#include <stdbool.h>


#define LINE_LENGTH 100 // Max # of characters in an input line
#define MAX_ARGS 7 // Max number of arguments to a command
#define MAX_LENGTH 20 // Max # of characters in an argument
#define NUM_SHELL_CMDS 7


/* Function prototypes for shell commands */
int jobs_cb(char *args[], process_table *pcb, bool file_input,
        bool file_output, char * input_file, char * output_file);
int exit_cb(char *args[], process_table *pc, bool file_input,
        bool file_output, char * input_file, char * output_fileb);
int kill_cb(char *args[], process_table *pcb, bool file_input,
        bool file_output, char * input_file, char * output_fileb);
int resume_cb(char *args[], process_table *pcb, bool file_input,
        bool file_output, char * input_file, char * output_file);
int suspend_cb(char *args[], process_table *pcb, bool file_input,
        bool file_output, char * input_file, char * output_file);
int wait_cb(char *args[], process_table *pcb, bool file_input,
        bool file_output, char * input_file, char * output_file);
int sleep_cb(char *args[], process_table *pcb, bool file_input,
        bool file_output, char * input_file, char * output_file);

#endif