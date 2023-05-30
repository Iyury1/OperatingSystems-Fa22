#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/time.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <stdbool.h>

/* User defined headers */
#include "fifo.h"
#include "tands.h"

/* User defines */
#define BUFFER_LENGTH       100
#define THREAD_POOL_SIZE    16
#define MAX_CONNECTIONS     500
#define PACKET_SIZE         1024
#define FD_MAX              1000
#define SERVER_TIMEOUT      30

/* User typedefs */
typedef struct client {
    char *clientname;
    int tran_count;
} client;

/* Private variables */
static pthread_mutex_t fifo_mutex, print_mutex, ttimer_mutex;
static pthread_cond_t full;
static int transaction_count = 0;
static fd_set current_sockets, ready_sockets;
static pthread_t thread_pool[THREAD_POOL_SIZE];
struct sockaddr_in address;
static FILE *fd;
static fifo queue;
static struct timeval server_timeout, start, end, current;
static struct itimerval server_timer;
static bool timed_out = false;
static bool first_transaction = true;
static int server_fd;
static client clients[FD_MAX*2];
static int client_ids[FD_MAX];
static int port;
static int client_count = 0;

/* Private function prototypes */
static void * thread_function(void *arg);
static void connection_handler(transaction* t);
static void setup_server();
static void usage_check(int argc, char* argv[]);
static void open_output_file();
static void receive_message(int i, int *bytes, char recvBuff[PACKET_SIZE], int recvBuff_size);
static void accept_connection(int *max_fd);
static void stimer_cb();
static void print_summary();
static void print_transaction(transaction *t);
static void print_done(transaction *t);


/*
* Main program
*
* Sets up a server to listen for incoming connections. Connected clients can
* request work to be completed as transactions which are executed by a thread
* pool running on the server.
*/
int main(int argc, char* argv[]) {

    // Check if command line arguments are correct
    usage_check(argc, argv);

    open_output_file();

    fprintf(fd, "Using port %s\n", argv[1]);

    // Initialize transaction queue
    fifo_init(&queue, 100);
    // Create Connection Thread Pool
    for(int i=0; i<THREAD_POOL_SIZE; i++) {
        pthread_create(&thread_pool[i], NULL, thread_function, NULL);
    }
    int bytes;
    char recvBuff[PACKET_SIZE];
    memset(recvBuff, 0, sizeof(recvBuff)); 
    memset(client_ids, -1, sizeof(client_ids)); 

    setup_server();

    // Largest file descriptor so far
    int max_fd = server_fd;
    /* Begin main loop */
    while(!timed_out) {
        // Reset server timer
        if(setitimer(ITIMER_REAL, &server_timer, NULL) < 0) {
            perror("Server timer failer");
            exit(EXIT_FAILURE);
        }
        // Select ready sockets
        ready_sockets = current_sockets;
        if (select(max_fd+1, &ready_sockets, NULL, NULL, NULL) < 0) {
            perror("Select error");
            if(timed_out) {
                break;
            } else {
                exit(EXIT_FAILURE);
            }
        }
        // Handle ready sockets
        for(int i=0; i<max_fd+1; i++) {
            if(FD_ISSET(i, &ready_sockets)) {
                if(i == server_fd) {
                    accept_connection(&max_fd);
                } else {
                    receive_message(i, &bytes, recvBuff, sizeof(recvBuff));
                }
            }
        }
    }
    /* Server timed out. Print summary to output file and terminate program. */
    printf("Server timed out, terminating program\n");
    print_summary();
    return 0;
}


/*
* Thread pool function
*
* Waits for work to appear in the work queue, and then executes the work
* as a transaction using the connection_handler
*/
static void * thread_function(void *arg) {
    while(true) {
        // Wait for work to become available
        pthread_mutex_lock(&fifo_mutex);
        if(pthread_cond_wait(&full, &fifo_mutex) != 0) {
            perror("Condition wait error");
            exit(EXIT_FAILURE);
        }
        // Get transaction from work queue
        transaction t;
        if(dequeue(&t, &queue) == false) {
            printf("queue was empty\n");
        }
        pthread_mutex_unlock(&fifo_mutex);
        // Execute transaction
        connection_handler(&t);
    }
}


/*
* Handler for a thread to execute transactions
*/
static void connection_handler(transaction* t) {
    /* Executing transaction */
    Trans(t->work);
    // Create Done message to send to client
    char done_message[12];
    int pos = 0;
    pos += sprintf(&done_message[pos], "%s", "D");
    pos += sprintf(&done_message[pos], "%d", t->count);
    sprintf(&done_message[pos], "%c", '\0');
    // Send Done message to client
    if(send(t->client_fd, done_message, strlen(done_message)+1, 0) != strlen(done_message)+1) {
        perror("Send \"Done\" message failed");
        exit(EXIT_FAILURE);
    }
    print_done(t);
}


/*
* Check command line arguments for correct format
*/
static void usage_check(int argc, char* argv[]) {
    // Usage check
    if(argc != 2) {
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
    }
    // Create filename string
    int pid = getpid();
    char pid_str[10];
    sprintf(pid_str, "%d", pid);
    char *filename = malloc(sizeof(char) * 40);
    memset(filename, 0, sizeof(char) * 40);
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


/*
* Sets up server socket to handle incoming connections
*/
static void setup_server() {
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(port);
    // Creating socket file descriptor
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket failed");
        exit(EXIT_FAILURE);
    }
    // Attach socket to the port 8080
    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }
    // Listen for connections
    if (listen(server_fd, MAX_CONNECTIONS) < 0) {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }
    // Initialize fdset
    FD_ZERO(&current_sockets);
    // Add server fd to fdset
    FD_SET(server_fd, &current_sockets);
    // Initalize timer for server timeout
    server_timeout.tv_sec = SERVER_TIMEOUT;
    server_timeout.tv_usec = 0;
    server_timer.it_interval = server_timeout;
    server_timer.it_value = server_timeout;
    if (signal(SIGALRM, stimer_cb) == SIG_ERR) {
        perror("Could not create signal handler for SIGALRM");
    }
}


/*
* Read the message from a connected socket and take appropriate response
*/
static void receive_message(int i, int *bytes, char recvBuff[PACKET_SIZE], int recvBuff_size) {
    *bytes = read(i, recvBuff, recvBuff_size-1);
    // Add null terminator to message
    recvBuff[*bytes] = 0;

    if(*bytes == 0) {
        /* Connection closed */
        FD_CLR(i, &current_sockets);
        client_ids[i] = -1;
    } else if(*bytes < 0) {
        perror("Read error");
        exit(EXIT_FAILURE);
    } else {
        if(recvBuff[0] == 'T') {
            /* Transaction Received */
            // Increment transaction counter for this client
            clients[client_ids[i]].tran_count += 1;
            // Get the work value as an integer
            int work = atoi(&recvBuff[1]);
            // Build transaction for work queue
            transaction t = {++transaction_count, i, work};
            // Print transaction to output file
            print_transaction(&t);
            // Place transaction on work queue and signal one thread
            // to process the transaction
            pthread_mutex_lock(&fifo_mutex);
            enqueue(t, &queue);
            pthread_mutex_unlock(&fifo_mutex);
            pthread_cond_signal(&full);
        } else if(recvBuff[0] == 'N' && client_ids[i] == -1) {
            /* Connection Received */
            // Add client to list of clients
            clients[client_count].clientname = malloc(sizeof(char) * 100);
            memcpy(clients[client_count].clientname, &recvBuff[1], 100);
            clients[client_count].tran_count = 0;
            client_ids[i] = client_count;
            client_count++;
        }
    }
}


/*
* Accept a connection from the server socket
*/
static void accept_connection(int *max_fd) {
    int client_fd = accept(server_fd, (struct sockaddr*)NULL, NULL);
    // Set the file descriptor bit in the fd_set of connected sockets
    FD_SET(client_fd, &current_sockets);
    if(client_fd > *max_fd) {
        // Set new max_fd
        *max_fd = client_fd;
    }
}


/*
* Signal handler for SIGALRM
*
* Sets a flag to indicate the server has timed out after not receiving a connection
* in SERVER_TIMEOUT seconds. This causes a break from the main while loop and the
* program will terminate after printing a summary to the output file.
*/
static void stimer_cb() {
    timed_out = true;
}


/*
* Prints summary to output file
*
* Print the number of transactions made by each client, and the average transactions
* per second.
*/
static void print_summary() {
    // Write to file
    pthread_mutex_lock(&print_mutex);
    fprintf(fd, "\nSUMMARY\n");
    for(int i=0; i<client_count; i++) {
        fprintf(fd, "  %d transactions from %s\n", clients[i].tran_count,
                clients[i].clientname);
    }
    // Get total elapsed time
    double total_time = (end.tv_sec - start.tv_sec) +
        ((end.tv_usec - start.tv_usec) / 1000000.0);
    // Write to file
    fprintf(fd, "%.2f transactions/second (%d/%.2f)",
            transaction_count / total_time, transaction_count, total_time);
    pthread_mutex_unlock(&print_mutex);
}


/*
* Prints transaction information to output file
*/
static void print_transaction(transaction *t) {
    // Get time of transaction
    double tran_time;
    if(first_transaction) {
        first_transaction = false;
        if(gettimeofday(&start, NULL) != 0) {
            perror("Get time of day failed");
            exit(1);
        }
        tran_time = (start.tv_sec) +
            (start.tv_usec / 1000000.0);
    } else {
        if(gettimeofday(&current, NULL) != 0) {
            perror("Get time of day failed");
            exit(1);
        }
        tran_time = (current.tv_sec) +
            (current.tv_usec / 1000000.0);
    }
    // Write transaction to file
    pthread_mutex_lock(&print_mutex);
    if(t->count < 10) {
        if(t->work < 10) {
            fprintf(fd, "%.2f #  %d (T  %d) from %s\n", tran_time, t->count,
                    t->work, clients[client_ids[t->client_fd]].clientname);
        } else if(t->work < 100) {
            fprintf(fd, "%.2f #  %d (T %d) from %s\n", tran_time, t->count,
                    t->work, clients[client_ids[t->client_fd]].clientname);
        } else {
            fprintf(fd, "%.2f #  %d (T%d) from %s\n", tran_time, t->count,
                    t->work, clients[client_ids[t->client_fd]].clientname);
        }
    } else if(transaction_count < 100) {
        if(t->work < 10) {
            fprintf(fd, "%.2f # %d (T  %d) from %s\n", tran_time, t->count,
                    t->work, clients[client_ids[t->client_fd]].clientname);
        } else if(t->work < 100) {
            fprintf(fd, "%.2f # %d (T %d) from %s\n", tran_time, t->count,
                    t->work, clients[client_ids[t->client_fd]].clientname);
        } else {
            fprintf(fd, "%.2f # %d (T%d) from %s\n", tran_time, t->count,
                    t->work, clients[client_ids[t->client_fd]].clientname);
        }
    } else {
        if(t->work < 10) {
            fprintf(fd, "%.2f #%d (T  %d) from %s\n", tran_time, t->count,
                    t->work, clients[client_ids[t->client_fd]].clientname);
        } else if(t->work < 100) {
            fprintf(fd, "%.2f #%d (T %d) from %s\n", tran_time, t->count,
                    t->work, clients[client_ids[t->client_fd]].clientname);
        } else {
            fprintf(fd, "%.2f #%d (T%d) from %s\n", tran_time, t->count,
                    t->work, clients[client_ids[t->client_fd]].clientname);
        }
    }

    pthread_mutex_unlock(&print_mutex);
}


/*
* Prints done information to output file
*/
static void print_done(transaction *t) {
    // Set time for most recently completed transaction
    pthread_mutex_lock(&ttimer_mutex);
    if(gettimeofday(&end, NULL) != 0) {
        perror("Get time of day failed");
        exit(EXIT_FAILURE);
    }
    double done_time = (end.tv_sec) +
                    (end.tv_usec / 1000000.0);
    pthread_mutex_unlock(&ttimer_mutex);
    // Write Done transaction to output file
    pthread_mutex_lock(&print_mutex);
    if(t->count < 10) {
        fprintf(fd, "%.2f #  %d (Done) from %s\n", done_time, t->count,
                clients[client_ids[t->client_fd]].clientname);
    } else if(t->count < 100) {
        fprintf(fd, "%.2f # %d (Done) from %s\n", done_time, t->count,
                clients[client_ids[t->client_fd]].clientname);
    } else {
        fprintf(fd, "%.2f #%d (Done) from %s\n", done_time, t->count,
                clients[client_ids[t->client_fd]].clientname);
    }

    pthread_mutex_unlock(&print_mutex);
}
