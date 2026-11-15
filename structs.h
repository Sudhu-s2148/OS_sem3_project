#ifndef STRUCTS_H
#define STRUCTS_H

#include <unistd.h>

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

typedef struct {
    int job_id;
    int arrival_time;
    int burst_time;      
    int remaining_time;  
    int completion_time; 
    pid_t pid;
    Status s;
} Job;
typedef struct{
    Job* queue[100];
    int q_front;
    int q_rear;
    int q_size;
}Queue;

#endif