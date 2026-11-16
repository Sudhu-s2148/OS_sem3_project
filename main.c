#include <stdio.h>
#include "dispatch.h"
#include "structs.h"
#include "logger_pthread.h"

int main() 
{
    pthread_t logger;
    pthread_create(&logger, NULL, logger_thread, NULL);
    
    //FCFS test
    Job fcfs_jobs[3] = {
        { .job_id = 1, .arrival_time = 0, .burst_time = 4, .remaining_time = 4, .pid = 0, .s = STATUS_WAITING },
        { .job_id = 2, .arrival_time = 2, .burst_time = 5, .remaining_time = 5, .pid = 0, .s = STATUS_WAITING },
        { .job_id = 3, .arrival_time = 5, .burst_time = 2, .remaining_time = 2, .pid = 0, .s = STATUS_WAITING }
    };

    //Round Robin test
    Job rr_jobs[3] = {
        { .job_id = 1, .arrival_time = 0, .burst_time = 4, .remaining_time = 4, .pid = 0, .s = STATUS_WAITING },
        { .job_id = 2, .arrival_time = 2, .burst_time = 5, .remaining_time = 5, .pid = 0, .s = STATUS_WAITING },
        { .job_id = 3, .arrival_time = 10, .burst_time = 2, .remaining_time = 2, .pid = 0, .s = STATUS_WAITING }
    };


    for (int i = 0; i < 3; i++)
    {
        dispatch_job_fcfs(&fcfs_jobs[i], 1);
    }

    run_round_robin(rr_jobs, 3, 2, 1);

    push_log("SHUTDOWN"); 
    pthread_join(logger, NULL);

    return 0;
}