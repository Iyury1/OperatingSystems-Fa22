#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

/* User defined headers */
#include "tands.h"

/* User defines */
#define LINE_LENGTH 100
#define ARG_COUNT 3
#define PACKET_SIZE 1024

/* Private variables */
static int port;
static char *filename;
static FILE *fd;
static struct timeval start;
static int work_count = 0;

/* Private function prototypes */
static void usage_check(int argc, char* argv[]);
static void check_input(char *in, int *n);
static void open_output_file();


/*
* Main program
*
* Sets up a client to connect to a server. Commands are read from user input
* and work can be requested to be completed by the server as transaction.
*/
int main(int argc, char* argv[]) {

    int sockfd = 0, client_fd;
    struct sockaddr_in serv_addr;

    usage_check(argc, argv);

    open_output_file();

    fprintf(fd, "Using port %s\n", argv[1]);
    fprintf(fd, "Using server address %s\n", argv[2]);
    fprintf(fd, "Host %s\n", filename);

    char recvBuff[PACKET_SIZE];
    
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation error");
        exit(EXIT_FAILURE);
    }
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);

    // Convert IPv4 and IPv6 addresses from text to binary
    if (inet_pton(AF_INET, argv[2], &serv_addr.sin_addr) <= 0) {
        perror("Invalid address/Address not supported");
        exit(EXIT_FAILURE);
    }

    if ((client_fd = connect(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr))) < 0) {
        perror("\nConnection Failed \n");
        exit(EXIT_FAILURE);
    }

    printf("Waiting 2 seconds for server to start\n");
    sleep(2);
    printf("Connection established\n");
    char clientname[101] = "N";
    strcat(clientname, filename);
    if(send(sockfd, clientname, strlen(clientname)+1, 0) != strlen(clientname)+1) {
        perror("Send failed");
        exit(EXIT_FAILURE);
    }
    /* Read input and execute commands */
    char in[LINE_LENGTH];
    while(fgets(in, LINE_LENGTH, stdin) != NULL) {
        int n;
        check_input(in, &n);
        if(in[0] == 'T') {
            if(gettimeofday(&start, NULL) != 0) {
                perror("Get time of day failed");
                exit(1);
            }
            double current_time = (start.tv_sec) +
                (start.tv_usec / 1000000.0);
            if(n < 10) {
                fprintf(fd, "%.2f Send (T  %d)\n", current_time, n);
            } else if(n < 100) {
                fprintf(fd, "%.2f Send (T %d)\n", current_time, n);
            } else {
                fprintf(fd, "%.2f Send (T%d)\n", current_time, n);
            }
            int bytes;
            if((bytes = send(sockfd, in, strlen(in)+1, 0)) != strlen(in)+1) {
                perror("Send failed");
                exit(EXIT_FAILURE);
            }
            work_count++;
            if(recv(sockfd, recvBuff, sizeof(recvBuff)-1, 0) < 0) {
                perror("Read failed");
                exit(EXIT_FAILURE);
            }
            if(gettimeofday(&start, NULL) != 0) {
                perror("Get time of day failed");
                exit(EXIT_FAILURE);
            }
            current_time = (start.tv_sec) +
                (start.tv_usec / 1000000.0);
            if(recvBuff[0] == 'D') {
                int m = atoi(&recvBuff[1]);
                if(m < 10) {
                    fprintf(fd, "%.2f Recv (D  %s)\n", current_time, &recvBuff[1]);
                } else if(m < 100) {
                    fprintf(fd, "%.2f Recv (D %s)\n", current_time, &recvBuff[1]);
                } else {
                    fprintf(fd, "%.2f Recv (D%s)\n", current_time, &recvBuff[1]);
                }
            } else {
                printf("Invalid message %c\n", recvBuff[0]);
                exit(EXIT_FAILURE);
            }
        } else if(in[0] == 'S'){
            fprintf(fd, "Sleep %d units\n", n);
            Sleep(n);
        } else {
            printf("Invalid message %c\n", in[0]);
            break;
        }
    }
    printf("No more input, closing connection\n");
    close(client_fd);
    fprintf(fd, "Sent %d transactions", work_count);
    return 0;
}


/*
* Check input
*
* Checks the validity of a given command and extracts the command type and work value
*/
static void check_input(char *in, int *n) {
    if(in[0] != 'T' && in[0] != 'S') {
        printf("Error: Invalid command provided\n");
        if(in[0] != ' ' && in[0] != '\r' && in[0] != '\n' && '\t') {
            exit(EXIT_FAILURE);
        }
    }
    char *endptr;
    *n = strtol(&in[1], &endptr, 0);
    if(errno != 0) {
        perror("strtol");
        exit(EXIT_FAILURE);
    }
    if(endptr == &in[1]) {
        fprintf(stderr, "No digits were found\n");
        exit(EXIT_FAILURE);
    }
    if(*endptr != '\0' && *endptr != '\n') {
        printf("Extra characters \"%s\" after integer\n", endptr);
        exit(EXIT_FAILURE);
    }
    if(in[0] == 'T') {
        if(*n < 0) {
            perror("Invalid integer provided for transaction, must be 'n > 0'");
            exit(1);
        }
    } else if(in[0] == 'S'){
        if(*n > 100 || *n < 0) {
            perror("Invalid integer provided for Sleep, must be '0 < n < 100'");
            exit(1);
        }
    }
}


/*
* Check command line arguments for correct format
*/
static void usage_check(int argc, char* argv[]) {
    // Usage check
    if(argc != ARG_COUNT) {
        printf("Error: Invalid number of arguments");
        exit(EXIT_FAILURE);
    }
    if((port = atoi(argv[1])) == 0) {
        printf("Error: port number is not a valid number");
        exit(EXIT_FAILURE);
    }
}


/*
* Open output file and create file if it does not exist
*/
static void open_output_file() {
    // Get host name as string
    char* hostname = NULL;
    hostname = malloc(sizeof(char) * 40);
    if(gethostname(hostname, 40) < 0) {
        perror("Could not get host name");
        exit(EXIT_FAILURE);
    }
    // Create filename string
    int pid = getpid();
    char pid_str[10];
    sprintf(pid_str, "%d", pid);
    filename = malloc(sizeof(char) * 100);
    memset(filename, 0, sizeof(char) * 100);
    strcat(filename, hostname);
    strcat(filename, ".");
    strcat(filename, pid_str);
    // Open file
    fd = fopen(filename, "w+");
    if(fd == NULL) {
        perror("Could not open output file");
        exit(EXIT_FAILURE);
    }
}

