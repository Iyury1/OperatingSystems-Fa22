#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <pthread.h>
#include <sys/time.h>
#include <unistd.h>
#include <sys/syscall.h>

/* User defined headers */
#include "fifo.h"
#include "tands.h"

/* User defined macros */
// https://stackoverflow.com/a/63827823
#ifndef SYS_gettid
#error "SYS_gettid unavailable on this system"
#endif
#define gettid() ((pid_t)syscall(SYS_gettid))

/* User defines */
#define LINE_LENGTH 100
#define PRODUCER_ID 0

/* Private function prototypes */
static void check_input(char *in, char *c, int *n);
static void * consume(void *arg);
static void print_message(int msg, int val, int id);
static void print_summary();

/* Private global variables */
static pthread_mutex_t count_mutex, print_mutex;
static pthread_cond_t empty, full;
static bool end_of_input = false;
static fifo queue;
static struct timeval start, end;
static int msg_stats[6] = {0, 0, 0, 0, 0, 0};
static int *thread_stats;
static int nthreads;
static FILE *fd;
enum message {Ask, Receive, Work, Complete, Tands_Sleep, End};


/*
* Main program
*
* Creates consumer threads and work queue. Runs producer thread.
*/
int main(int argc, char *argv[]) {

  // Get program start time
  if(gettimeofday(&start, NULL) != 0) {
    perror("Get time of day failed");
    exit(1);
  }

  // Process command arguments
  if(argc < 2) {
    printf("Error: Not enough command line arguments\n");
    exit(1);
  }
  nthreads = atoi(argv[1]);
  char filenum[100];
  char *filename = malloc(sizeof(char) * 100);
  thread_stats = malloc(sizeof(int) * nthreads);
  memset(thread_stats, 0, sizeof(int) * nthreads);

  // Create file output
  strcat(filename, "prodcon.");
  if(argc > 2) {
    sprintf(filenum, "%d", atoi(argv[2]));
    strcat(filename, filenum);
    strcat(filename, ".");
  }
  strcat(filename, "log");
  fd = fopen(filename, "w+");
  if(fd == NULL) {
    perror("Could not open output file");
    exit(1);
  }

  // Create work queue and consumer threads
  int queue_size = nthreads * 2;
  fifo_init(&queue, queue_size);
  pthread_t consumers[nthreads];
  int ids[nthreads];
  for(int i=0; i<nthreads; i++) {
    ids[i] = i+1;
    pthread_create(&consumers[i], NULL, &consume, (void *)&ids[i]);
  }
  
  /*
  * Read input and execute commands. This is the producer thread task. The producer
  * reads commands and either Sleeps or adds a work item to the work queue. The work
  * queue is provided with mutual exclusion.
  */
  char in[LINE_LENGTH];
  char c;
  int n;
  while(fgets(in, LINE_LENGTH, stdin) != NULL) {
    c = in[0];
    check_input(in, &c, &n);
    if(c == 'S') {  
      print_message(Tands_Sleep, n, PRODUCER_ID);
      Sleep(n);
    } else if(c == 'T') {
      if(pthread_mutex_lock(&count_mutex) != 0) {
        perror("Mutex lock error");
        exit(1);
      }
      while(fifo_full(&queue)) {
        // Cannot add work until a consumer finishes some existing work
        if(pthread_cond_wait(&empty, &count_mutex) != 0) {
          perror("Condition wait error");
          exit(1);
        }
      }
      // Add work to FIFO
      print_message(Work, n, PRODUCER_ID);
      enqueue(n, &queue);
      // Notify consumers of available work      
      if(pthread_cond_broadcast(&full) != 0) {
        perror("Condition broadcast error");
        exit(1);
      }
      if(pthread_mutex_unlock(&count_mutex) != 0) {
        perror("Mutex unlock error");
        exit(1);
      }      
    }
  }
  print_message(End, 0, 0);
  end_of_input = true;
  // Program input has ended, notify consumer threads
  if(pthread_cond_signal(&full) != 0) {
    perror("Condition signal error");
    exit(1);
  }
  // Wait for consumer threads to complete their work before ending program
  int status;
  for (int i=0; i<nthreads; i++) {
    status = pthread_join(consumers[i], NULL);
    if (status != 0) {
      printf("Pthread join failed\n");
      exit(1);
    }
  }
  print_summary();
  // Deallocate memory from heap
  fifo_deinit(&queue);
  free(filename);
  free(thread_stats);
  fclose(fd);
  return 0;
}


/*
* Check input
*
* Checks the validity of a given command and extracts the command type and work value
*/
void check_input(char *in, char *c, int *n) {
  if(*c != 'S' && *c != 'T') {
    printf("Error: Invalid command provided\n");
    exit(1);
  }
  memcpy(in, in+1,strlen(in));
  if(in == NULL) {
    *n = 0;
  } else if((*n = atoi(in)) == 0) {
    printf("Error invalid integet provided\n");
    exit(1);
  }
  // if(*n < 0) {
  //   printf("Error: Incorrect integer provided\n");
  //   exit(1);
  // }
}


/*
* Consumer thread task
*
* Reads from the work FIFO as work becomes available. The FIFO is provided
* with mutual exclusion using the count_mutex. When the EOF has been detected
* from the program input, each thread will exit when all of the work has been 
* completed.
*/
void * consume(void *arg) {
  int *id = (int *)arg;
  while(true) {
    // Ask for work
    print_message(Ask, 0, *id);
    if(pthread_mutex_lock(&count_mutex) != 0) {
      perror("Mutex lock error");
      exit(1);
    }
    while(fifo_empty(&queue)) {
      if(end_of_input) {
        // EOF detected, and no remaining work. Thread can exit.
        if(pthread_mutex_unlock(&count_mutex) != 0) {
          perror("Mutex unlock error");
          exit(1);
        }
        if(pthread_cond_signal(&full) != 0) {
          perror("Condition signal error");
          exit(1);
        }
        pthread_exit(NULL);
      }
      if(pthread_cond_wait(&full, &count_mutex) != 0) {
        perror("Condition wait error");
        exit(1);
      }
    }
    // Receive work
    int work = dequeue(&queue);
    print_message(Receive, work, *id);
    if(pthread_cond_signal(&empty) != 0) {
      perror("Condition signal error");
      exit(1);
    }
    if(pthread_mutex_unlock(&count_mutex) != 0) {
      perror("Mutex unlock error");
      exit(1);
    }
    // Complete work
    Trans(work);
    print_message(Complete, work, *id);
  }
}


/*
* Print summary
*
* Print a status message to file output along with calling thread id and work value.
* This function is provided mutual exclusion since the file descriptor is written to
* by multiple threads, as well as the end variable used for calculating current_time.
*/
void print_message(int msg, int n, int id) {
  if(pthread_mutex_lock(&print_mutex) != 0) {
    perror("Mutex lock error");
    exit(1);
  }
  // Check if msg is valid
  if(msg != Ask && msg != Receive && msg != Work && msg != Complete && msg != Tands_Sleep && msg != End) {
    printf("Error: Invalid message type");
    if(pthread_mutex_unlock(&print_mutex) != 0) {
      perror("Mutex unlock error");
      exit(1);
    }
    return;
  }
  // Update msg_stats[] and thread_stats[]
  msg_stats[msg] += 1;
  if(msg == Complete && id != 0) {
    thread_stats[id-1] += 1;
  }
  // Get total elapsed time
  if(gettimeofday(&end, NULL) != 0) {
    perror("Get time of day failure");
    exit(1);
  }
  double current_time = (end.tv_sec - start.tv_sec) +
        ((end.tv_usec - start.tv_usec) / 1000000.0);
  // Write to file
  fprintf(fd, "   %.3f", current_time);
  fprintf(fd, " ID= %d", id);
  switch(msg) {
  case Ask:
    fprintf(fd, "      Ask\n");
    break;
  case Receive:
    fprintf(fd, " Q= 0 Receive      %d\n", n); 
    break;
  case Work:
    fprintf(fd, " Q= 0 Work         %d\n", n); 
    break;
  case Complete:
    fprintf(fd, "      Complete     %d\n", n);
    break;
  case Tands_Sleep:
    fprintf(fd, "      Sleep        %d\n", n);
    break;
  case End:
    fprintf(fd, "      End\n");
    break;
  default:
    fprintf(fd, "Called print_message with msg = %d\n", msg);
  }
  if(pthread_mutex_unlock(&print_mutex) != 0){
    perror("Mutex unlock error");
    exit(1);
  }
}


/*
* Print summary
*
* Print the contents of msg_stats[] and thread_stats[] to file output
*/
void print_summary() {
  if(gettimeofday(&end, NULL) != 0) {
    perror("Get time of day failed");
    exit(1);
  }
  // Get total elapsed time
  double current_time = (end.tv_sec - start.tv_sec) +
        ((end.tv_usec - start.tv_usec) / 1000000.0);
  // Write to file
  fprintf(fd, "Summary:\n");
  if(msg_stats[Work] > 999) {
    fprintf(fd, "    Work       %d\n", msg_stats[Work]);
  } else if (msg_stats[Work] > 99) {
    fprintf(fd, "    Work        %d\n", msg_stats[Work]);
  } else if (msg_stats[Work] > 9) {
    fprintf(fd, "    Work         %d\n", msg_stats[Work]);
  } else {
    fprintf(fd, "    Work          %d\n", msg_stats[Work]);
  }
  if(msg_stats[Ask] > 999) {
    fprintf(fd, "    Ask        %d\n", msg_stats[Ask]);
  } else if (msg_stats[Ask] > 99) {
    fprintf(fd, "    Ask         %d\n", msg_stats[Ask]);
  } else if (msg_stats[Ask] > 9) {
    fprintf(fd, "    Ask          %d\n", msg_stats[Ask]);
  } else {
    fprintf(fd, "    Ask           %d\n", msg_stats[Ask]);
  }
  if(msg_stats[Receive] > 999) {
    fprintf(fd, "    Receive    %d\n", msg_stats[Receive]);
  } else if (msg_stats[Receive] > 99) {
    fprintf(fd, "    Receive     %d\n", msg_stats[Receive]);
  } else if (msg_stats[Receive] > 9) {
    fprintf(fd, "    Receive      %d\n", msg_stats[Receive]);
  } else {
    fprintf(fd, "    Receive       %d\n", msg_stats[Receive]);
  }
  if(msg_stats[Complete] > 999) {
    fprintf(fd, "    Complete   %d\n", msg_stats[Complete]);
  } else if (msg_stats[Complete] > 99) {
    fprintf(fd, "    Complete    %d\n", msg_stats[Complete]);
  } else if (msg_stats[Complete] > 9) {
    fprintf(fd, "    Complete     %d\n", msg_stats[Complete]);
  } else {
    fprintf(fd, "    Complete      %d\n", msg_stats[Complete]);
  }
  if(msg_stats[Tands_Sleep] > 999) {
    fprintf(fd, "    Sleep      %d\n", msg_stats[Tands_Sleep]);
  } else if (msg_stats[Tands_Sleep] > 99) {
    fprintf(fd, "    Sleep       %d\n", msg_stats[Tands_Sleep]);
  } else if (msg_stats[Tands_Sleep] > 9) {
    fprintf(fd, "    Sleep        %d\n", msg_stats[Tands_Sleep]);
  } else {
    fprintf(fd, "    Sleep         %d\n", msg_stats[Tands_Sleep]);
  }
  float total_trans = 0;
  for(int i=0; i<nthreads; i++) {
    total_trans += thread_stats[i];
    if(i > 998) {
      fprintf(fd, "    Thread  %d  %d\n", i+1, thread_stats[i]);
    } else if(i > 98) {
      fprintf(fd, "    Thread  %d   %d\n", i+1, thread_stats[i]);
    } else if(i > 8) {
      fprintf(fd, "    Thread  %d    %d\n", i+1, thread_stats[i]);
    } else {
      fprintf(fd, "    Thread  %d     %d\n", i+1, thread_stats[i]);
    }
  }
  fprintf(fd, "Transactions per second: %.2f\n", total_trans / current_time);
}
