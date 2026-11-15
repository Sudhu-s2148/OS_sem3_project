#ifndef STRUCTS_H
#define STRUCTS_H

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
    STATUS_DONE
}Status;

typedef struct{
    int job_id;
    int arrival_time;
    int burst_time;
    Priority p;
    Status s;
    int max_claim[NUM_RESOURCES];   /* Maximum units of each resource the job may ever need */
    int allocated[NUM_RESOURCES];   /* Units of each resource the job currently holds */
}Job;

#endif