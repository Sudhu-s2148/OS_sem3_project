#include "logger_pthread.h"
#include <stdio.h>
#include <string.h>

char log_queue[32][4096];
int q_count = 0;
int q_head = 0;
int q_tail = 0;

pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER; //initialise lock
pthread_cond_t cond_not_full = PTHREAD_COND_INITIALIZER; //initialise condition variables
pthread_cond_t cond_not_empty = PTHREAD_COND_INITIALIZER;

void push_log(const char* message) 
{
    //acquire mutex lock
    pthread_mutex_lock(&lock);
    
    //check whether queue is full, if full wait on this push 
    //if queue is full, the writer just holds the lock forever, and logger cant read from it
    //pthread_cond_wait unlocks mutex so logger can acquire and read from queue 
    while (q_count == 5) 
    {
        pthread_cond_wait(&cond_not_full, &lock);
    }
    
    //push message to log queue
    strcpy(log_queue[q_tail], message);
    q_tail = (q_tail + 1) % 5;
    q_count++;
    
    //if queue is empty, logger is sleeping (no work to be done)
    //pthread_cond_signal wakes up logger because a message was just pushed to log queue
    pthread_cond_signal(&cond_not_empty);

    //release mutex lock
    pthread_mutex_unlock(&lock);
}

//process to run on logger thread
void* logger_thread(void* arg) 
{
    (void)arg; //silence compiler warning
    //get time to print to log
    struct timeval start_time, current_time;
    gettimeofday(&start_time, NULL);

    //open log file
    FILE *log_file = fopen("scheduler_events.log", "w");
    if (log_file == NULL) 
    {
        perror("Failed to open log file");
        return NULL; 
    }

    while (1) 
    {
        //acquire mutex lock
        pthread_mutex_lock(&lock);
        
        //if queue is empty, logger waits
        //when queue is empty, if logger has acquired mutex, it cant do anything, as there are no messages to log
        //pthread_cond_wait puts logger to sleep and releases mutex so writers can push to log queue
        while (q_count == 0) 
        {
            pthread_cond_wait(&cond_not_empty, &lock);
        }
        
        //check for the shutdown signal
        if (strcmp(log_queue[q_head], "SHUTDOWN") == 0) 
        {
            pthread_mutex_unlock(&lock);
            break; 
        }

        gettimeofday(&current_time, NULL);
        
        //calculate elapsed time in seconds (with millisecond precision)
        double elapsed = (current_time.tv_sec - start_time.tv_sec) + (current_time.tv_usec - start_time.tv_usec) / 1000000.0;
        
        //print message to log file
        fprintf(log_file, "[%.3f sec] %s\n", elapsed, log_queue[q_head]);
        //flush the buffer to ensure logs appear immediately in the file
        fflush(log_file);
        
        q_head = (q_head + 1) % 5;
        q_count--;
        
        //if queue is full, no writers can push logs
        //signalling cond_not_full tells writers that a space on the log queue has been freed 
        //and they can write to it
        pthread_cond_signal(&cond_not_full);

        //release mutex lock
        pthread_mutex_unlock(&lock);
    }

    fclose(log_file);

    printf("outputs logged, logger exiting\n");

    return NULL;
}