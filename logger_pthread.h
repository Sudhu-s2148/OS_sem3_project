#ifndef LOGGER_PTHREAD_H
#define LOGGER_PTHREAD_H

#include <pthread.h>
#include <sys/time.h>

extern char log_queue[32][4096]; //queue of log messages
extern int q_count; 
extern int q_head;
extern int q_tail;

extern pthread_mutex_t lock; //mutex lock
extern pthread_cond_t cond_not_full; //pthread_cond_t is a condition variable, can be used to notify threads when a state changes
extern pthread_cond_t cond_not_empty;

void push_log(const char* message); //push message to log queue
void* logger_thread(void* arg); //process to run on the logger posix thread

#endif