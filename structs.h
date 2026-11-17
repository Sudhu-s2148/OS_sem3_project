#ifndef STRUCTS_H
#define STRUCTS_H

#include <unistd.h>
/* Global resource model used by the Banker's algorithm.
   Resource types R0, R1, R2 with these total system capacities. */
#define NUM_RESOURCES 3
static const int RESOURCE_TOTAL[NUM_RESOURCES] = {9, 12, 15};

typedef enum{
    PRIORITY_LOW,
    PRIORITY_MEDIUM,
    PRIORITY_HIGH
}Priority;

typedef enum{
    STATUS_WAITING,
    STATUS_RUNNING,
    STATUS_DONE,
    STATUS_FAILED
}Status;

typedef struct{
    int job_id;
    int arrival_time;
    int burst_time;
    int remaining_time;
    int completion_time;
    pid_t pid;
    Priority p;
    Status s;
    int max_claim[NUM_RESOURCES];   /* Maximum units of each resource the job may ever need */
    int allocated[NUM_RESOURCES];   /* Units of each resource the job currently holds */
}Job;

typedef struct{
    Job* queue[100];
    int q_front;
    int q_rear;
    int q_size;
}Queue;

#endif