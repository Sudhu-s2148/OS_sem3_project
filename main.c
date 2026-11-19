#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include "structs.h"
#include "dispatch.h"
#include "logger_pthread.h" 
#include "banker.h"
#include "ipc.h"


int main(int argc, char *argv[]) 
{
    //flags for which scheduler to use and whether to log or not
    int run_f = 0;
    int run_rr = 0;
    int run_p = 0;
    int run_b = 0;
    int run_ub = 0;
    int use_log = 0;
    int ipc = 0;
    int disp = 0;

    for (int i = 1; i < argc; i++) 
    {
        if (strcmp(argv[i], "-fcfs") == 0) run_f = 1;
        else if (strcmp(argv[i], "-rr") == 0) run_rr = 1;
        else if (strcmp(argv[i], "-prio") == 0) run_p = 1;
        else if (strcmp(argv[i], "-banker") == 0) run_b = 1;
        else if (strcmp(argv[i], "-ubanker") == 0) run_ub = 1;
        else if (strcmp(argv[i], "-ipc") == 0) ipc = 1;
        else if (strcmp(argv[i], "-log") == 0) use_log = 1;
        else if (strcmp(argv[i], "-disp") == 0) disp = 1;
        else if (strcmp(argv[i], "-all") == 0)
        {
            run_f = 1;
            run_rr = 1;
            run_p = 1;
            run_b = 1;
            run_ub = 1;
            ipc = 1;
            disp = 1;   
        }
    }

    //if no algorithms were selected, show usage instructions and exit
    if (run_f + run_rr + run_p + run_b + ipc + use_log + disp  + run_ub == 0) 
    {
        printf("Usage: ./scheduler [-fcfs] [-rr] [-prio] [-banker] [-log] [-disp]\n");
        printf("Example: ./scheduler -fcfs -rr -log\n");
        return 0;
    }

    //start logger thread
    pthread_t logger;
    pthread_create(&logger, NULL, logger_thread, NULL);

    //RESOURCE_TOTAL capacity is {9, 12, 15}
    Job base_jobs[4] = {
        { .job_id = 1, .arrival_time = 0, .burst_time = 3,  .remaining_time = 3,  .p = PRIORITY_HIGH,   
          .max_claim = {3, 2, 2}, .allocated = {1, 0, 0}, .pid = 0, .s = STATUS_WAITING },
        
        { .job_id = 2, .arrival_time = 2, .burst_time = 4,  .remaining_time = 4,  .p = PRIORITY_MEDIUM, 
          .max_claim = {6, 1, 3}, .allocated = {4, 1, 1}, .pid = 0, .s = STATUS_WAITING },
        
        { .job_id = 3, .arrival_time = 4, .burst_time = 10, .remaining_time = 10, .p = PRIORITY_LOW,    
          .max_claim = {3, 1, 4}, .allocated = {2, 1, 1}, .pid = 0, .s = STATUS_WAITING }, 
        
        { .job_id = 4, .arrival_time = 6, .burst_time = 2,  .remaining_time = 2,  .p = PRIORITY_HIGH,   
          .max_claim = {4, 2, 2}, .allocated = {0, 0, 2}, .pid = 0, .s = STATUS_WAITING }
    };

    Job unsafe_jobs[3] = {
        { .job_id = 1, .arrival_time = 0, .burst_time = 3, .remaining_time = 3, .p = PRIORITY_HIGH,   
          .max_claim = {6, 8, 8}, .allocated = {4, 6, 6}, .pid = 0, .s = STATUS_WAITING },
        
        { .job_id = 2, .arrival_time = 2, .burst_time = 4, .remaining_time = 4, .p = PRIORITY_MEDIUM, 
          .max_claim = {8, 7, 7}, .allocated = {4, 5, 5}, .pid = 0, .s = STATUS_WAITING },
        
        { .job_id = 3, .arrival_time = 4, .burst_time = 5, .remaining_time = 5, .p = PRIORITY_LOW,    
          .max_claim = {4, 3, 5}, .allocated = {1, 0, 3}, .pid = 0, .s = STATUS_WAITING }
    };



    if (disp)
    {
        display_jobs(base_jobs, 4);
    }

    if (ipc)
    {
        printf("Starting IPC/shared memory test: \n\n");
        test_ipc();
        printf("\nIPC/shared memory test complete\n\n");
    }

    if (run_b) 
    {
        BankerResult b_result;
        
        //run the banker's algorithm
        if (banker_check(base_jobs, 4, &b_result) == 0) 
        {
            char banker_buffer[2048];
            banker_print_buffer(&b_result, banker_buffer);

            if (use_log) 
            {
                push_log(banker_buffer);
            } 
            else 
            {
                printf("%s\n", banker_buffer);
            }
        }
        else 
        {
            printf("Banker's check failed to execute.\n");
        }
    }

    if (run_ub)
    {
        BankerResult b_result;
        
        //run the banker's algorithm
        if (banker_check(unsafe_jobs, 3, &b_result) == 0) 
        {
            char banker_buffer[2048];
            banker_print_buffer(&b_result, banker_buffer);

            if (use_log) 
            {
                push_log(banker_buffer);
            } 
            else 
            {
                printf("%s\n", banker_buffer);
            }
        }
        else 
        {
            printf("Banker's check failed to execute.\n");
        }    
    }

    //execute Selected Schedulers
    if (run_f) 
    {        
        Job fcfs_jobs[4];
        memcpy(fcfs_jobs, base_jobs, sizeof(base_jobs)); // Fresh copy
        
        run_fcfs(fcfs_jobs, 4, use_log);
    }

    if (run_rr) 
    {        
        Job rr_jobs[4];
        memcpy(rr_jobs, base_jobs, sizeof(base_jobs)); // Fresh copy
        
        run_round_robin(rr_jobs, 4, 2, use_log); // Using a quantum of 2
    }

    if (run_p) 
    {        
        Job prio_jobs[4];
        memcpy(prio_jobs, base_jobs, sizeof(base_jobs)); // Fresh copy
        
        run_priority_queue(prio_jobs, 4, use_log);
    }

    //shut down logger thread
    push_log("SHUTDOWN");
    pthread_join(logger, NULL);

    return 0;
}