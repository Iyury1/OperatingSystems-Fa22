#ifndef __SHELL_ERR_H_
#define __SHELL_ERR_H__

#define NUM_SHELL_ERRORS        18

/* Command error codes */       
#define CMD_OK                  0
#define CMD_MAX_LENGTH_ERR      1
#define CMD_MAX_ARG_ERR         2
#define CMD_ARGS_ERR            3
#define CMD_PROC_NOT_FND        4
#define CMD_PROC_NOT_SUSP       5
#define CMD_PROC_NOT_RUN        6
#define CMD_EXIT_FAIL           7

/* Process error codes */
#define PROC_OK                 0
#define PROC_TABLE_FULL         8

/* System error codes */
#define SYS_POPEN_FAIL          9
#define SYS_PCLOSE_FAIL         10
#define SYS_FORK_FAIL           11
#define SYS_EXEC_FAIL           12
#define SYS_WAIT_FAIL           13 
#define SYS_DUP2_FAIL           14
#define SYS_CLOSE_FAIL          15
#define SYS_DUP_FAIL            16
#define SYS_KILL_FAIL           17


/* Macro to print error number and message */
#define CHECK_CMD_ERR(errno, msg) do { \
  int retval = (errno); \
  if (errno != 0) { \
    fprintf(stderr, "Runtime error: %s returned %d  -> %s\n", #errno, retval, msg); \
  } \
} while (0)

extern char * err_strings[NUM_SHELL_ERRORS];

#endif