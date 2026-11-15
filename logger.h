#ifndef LOGGER_H
#define LOGGER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <semaphore.h>

#define QUEUE_SIZE 5
#define MAX_MSG_LEN 256
#define SHM_NAME "/shm"
#define SEM_MUTEX_NAME "/sem_mutex"
#define SEM_EMPTY_NAME "/sem_empty"
#define SEM_FULL_NAME "/sem_full"


typedef struct {
    char log_queue[QUEUE_SIZE][MAX_MSG_LEN];
    int q_head;
    int q_tail;
    int q_count;
    int active_producers;
} SharedData;

void test_logger();

#endif