#include "dispatch.h"

void execute_worker(Job* job, int log)
{
    struct rlimit rl;
    //set cpu time limit as 2 * burst time
    rl.rlim_cur = 2 * job->burst_time;
    rl.rlim_max = 4 * job->burst_time;

    //enforce cpu time limit
    if (setrlimit(RLIMIT_CPU, &rl) != 0) 
    {
        perror("Failed to set CPU limit");
    }

    if (log == 0)
        printf("Worker pid: %d | job %d starting | time left: %d\n", getpid(), job->job_id, job->burst_time);

    //simulate failure
    if (job->job_id == 2)
    {
        int a = 1/0;
    }
    else if (job->job_id == 3)
    {
        while(1)
        {
        
        }
    }
    else 
    {
        for(int i = 0; i < job->burst_time; i++) 
        {
            clock_t start_time = clock();
            while ((clock() - start_time) / (double)CLOCKS_PER_SEC < 1.00) 
            {
                //do nothing, just burn cpu cycles
            }
        }
    }
    
    
    if (log == 0)
        printf("Worker pid: %d | job %d completed its work\n", getpid(), job->job_id);

    exit(0);
}

void dispatch_job_fcfs(Job* job, int log)
{
    job->s = STATUS_RUNNING;

    //fork new process for job
    job->pid = fork();

    if (job->pid < 0)
    {
        perror("fork failed in scheduler");
    }

    //child process (worker) branch
    if (job->pid == 0)
    {
        execute_worker(job, log);
    }
    //parent branch
    else 
    {
        if (log == 1)
        {
            char start_msg[256];
            sprintf(start_msg, "Job %d starting | time left: %d\n", job->job_id, job->burst_time);
            push_log(start_msg);
        }

        int status;

        waitpid(job->pid, &status, 0);

        char msg[256];

        if (WIFEXITED(status)) //check if child terminated properly
        {
            job->s = STATUS_DONE;
            if (log == 1)
                sprintf(msg, "Job %d completed successfully\n\n", job->job_id);
        }
        else if (WIFSIGNALED(status)) //check if child sent some signal
        {
            job->s = STATUS_FAILED;
            if (log == 1)
                sprintf(msg, "job %d failed with signal: %d\n\n", job->job_id, WTERMSIG(status));
            else 
                printf("job %d failed with signal: %d\n", job->job_id, WTERMSIG(status)); 
        }

        push_log(msg);
    }
}

void execute_worker_round_robin(Job* job) 
{
    struct rlimit rl;
    rl.rlim_cur = 4; 
    rl.rlim_max = 6; 
    
    if (setrlimit(RLIMIT_CPU, &rl) != 0) 
    {
        perror("Failed to set CPU limit");
    }
    //simulate failure on the 2nd time quantum of 2nd job
    if (job->job_id == 2)
    {
        int a = 1/0;
    }
    else if (job->job_id == 3) //simulate timeout
    {
        while(1)
        {

        }
    }
    else 
    {
        for(int i = 0; i < job->remaining_time; i++) 
        {
            clock_t start_time = clock();
            // Spin the CPU intensely for exactly 0.95 seconds
            while ((clock() - start_time) / (double)CLOCKS_PER_SEC < 0.95) 
            {
                // do nothing, just burn CPU cycles
            }
        }
    }
    exit(0);
}

Queue* create_ready_queue()
{
    Queue* r = malloc(sizeof(Queue));
    r->q_front = 0;
    r->q_rear = 0;
    r->q_size = 0;

    return r;
}


void enqueue(Job* j, Queue* rq) 
{
    rq->queue[rq->q_rear++] = j;
    rq->q_size++;
}

Job* dequeue(Queue* rq) 
{
    rq->q_size--;
    return rq->queue[rq->q_front++];   
}


void run_round_robin(Job jobs[], int num_jobs, int quantum, int log) 
{

    Queue* rq = create_ready_queue();

    int active_jobs = num_jobs;
    int current_time = 0;
    int unarrived_idx = 0; 
    
    char msg[256];
    if (log == 1)
    {
        sprintf(msg, "Starting Round Robin (Quantum = %d)\n", quantum);
        push_log(msg);
        memset(msg, 0, sizeof(msg));
    } 
    else 
    {
        printf("\nStarting Round Robin (Quantum = %d)\n", quantum);
    }
    
    

    //enqueue any jobs that arrive initially
    while(unarrived_idx < num_jobs && jobs[unarrived_idx].arrival_time <= current_time) 
    {
        if (log == 1)
        {
            sprintf(msg, "[Time %d] Job %d arrived and joined the queue.\n", current_time, jobs[unarrived_idx].job_id);  
            push_log(msg);
            memset(msg, 0, sizeof(msg));
        }
        else 
        {
            printf("[Time %d] Job %d arrived and joined the queue.\n", current_time, jobs[unarrived_idx].job_id);
        }
        enqueue(&jobs[unarrived_idx], rq);
        unarrived_idx++;
    }


    while (active_jobs > 0) 
    {
        //queue is empty, but jobs are still pending (yet to arrive)
        if (rq->q_size == 0 && unarrived_idx < num_jobs) 
        {
            if (log == 1)
            {
                sprintf(msg, "[Time %d] CPU Idle. Fast forwarding to next arrival.\n", current_time);
                push_log(msg);
                memset(msg, 0, sizeof(msg));
            }
            else 
            {
                printf("[Time %d] CPU Idle. Fast forwarding to next arrival.\n", current_time);
            }
            current_time = jobs[unarrived_idx].arrival_time;
            
            //job arrives
            while(unarrived_idx < num_jobs && jobs[unarrived_idx].arrival_time <= current_time) 
            {
                if (log == 1)
                {
                    sprintf(msg, "[Time %d] Job %d arrived and joined the queue.\n", current_time, jobs[unarrived_idx].job_id);  
                    push_log(msg);
                    memset(msg, 0, sizeof(msg));
                }
                else 
                {
                    printf("[Time %d] Job %d arrived and joined the queue.\n", current_time, jobs[unarrived_idx].job_id);
                }
                enqueue(&jobs[unarrived_idx], rq);
                unarrived_idx++;
            }
        }


        //dequeue the next job in queue and dispatch with fork()
        Job* j = dequeue(rq);
        if (log)
        {
            sprintf(msg, "[Time %d] Dispatching Job %d\n", current_time, j->job_id);
            push_log(msg);
            memset(msg, 0, sizeof(msg));
        }
        else 
        {
            printf("[Time %d] Dispatching Job %d\n", current_time, j->job_id);
        }

        if (j->pid == 0) 
        { 
            j->pid = fork();
            if (j->pid == 0) execute_worker_round_robin(j);
        } 
        else 
        {   
            //kill with SIGCONT basically tells that process to resume
            kill(j->pid, SIGCONT);
        }
        j->s = STATUS_RUNNING;
        

        //monitor for time quantum expiring and new arrivals
        int time_run = 0;
        int status;
        pid_t result = 0;

        while (time_run < quantum) 
        {
            result = waitpid(j->pid, &status, WNOHANG); //wnohang is a non blocking check
            if (result > 0) break; //job finished before time quantum expired
            
            sleep(1); //advance real time (worker time is slightly less to prevent race condition)
            time_run++;
            current_time++; 
            
            //tiebreak between job arriving exactly when new quantum starts, and job which was waiting (new arrival goes first)
            while(unarrived_idx < num_jobs && jobs[unarrived_idx].arrival_time == current_time)     
            {
                if (log == 1)
                {
                    sprintf(msg, "[Time %d] Job %d arrived and joined the queue.\n", current_time, jobs[unarrived_idx].job_id);  
                    push_log(msg);
                    memset(msg, 0, sizeof(msg));
                }
                else 
                {
                    printf("[Time %d] Job %d arrived and joined the queue.\n", current_time, jobs[unarrived_idx].job_id);
                }
                enqueue(&jobs[unarrived_idx], rq);
                unarrived_idx++;
            }
        }

        if (result == 0) 
        {   
            //check in case worker finishes at the exact last second (should not happen because worker's time is slightly less)
            result = waitpid(j->pid, &status, WNOHANG);
            
            if (result == 0) 
            {
                //kill with sigstop is pausing the process
                kill(j->pid, SIGSTOP); 
                j->s = STATUS_WAITING;
            
                j->remaining_time -= time_run; 
                
                if (log == 1)
                {
                    sprintf(msg, "[Time %d] Time quantum up. Job %d paused (%d units left).\n", current_time, j->job_id, j->remaining_time);
                    push_log(msg);
                    memset(msg, 0, sizeof(msg));
                }
                else 
                {
                    printf("[Time %d] Time quantum up. Job %d paused (%d units left).\n", current_time, j->job_id, j->remaining_time);
                }
                
                //enqueue paused job at the back
                enqueue(j, rq); 
            }
        }
        
        if (result > 0) 
        {
            if (WIFEXITED(status)) 
            {
                j->s = STATUS_DONE;
                
                j->completion_time = current_time; 
                
                if (log == 1)
                {
                    sprintf(msg, "[Time %d] Job %d completed successfully.\n", current_time, j->job_id); 
                    push_log(msg);
                    memset(msg, 0, sizeof(msg));  
                }
                else 
                {
                    printf("[Time %d] Job %d completed successfully.\n", current_time, j->job_id);
                }
                
            }
            //check for failure
            else if (WIFSIGNALED(status)) 
            {
                j->s = STATUS_FAILED;
                j->completion_time = current_time;
                if (log == 1)
                {
                    sprintf(msg, "[Time %d] Job %d FAILED with signal %d.\n", current_time, j->job_id, WTERMSIG(status));
                    push_log(msg);
                    memset(msg, 0, sizeof(msg));    
                }
                else 
                {
                    printf("[Time %d] Job %d FAILED with signal %d.\n", current_time, j->job_id, WTERMSIG(status));
                }   
            }
            active_jobs--; 
        }
    }

    if (log == 1)
    {
        sprintf(msg, "All Jobs Completed (Total Time: %d)\n", current_time);  
        push_log(msg);
        memset(msg, 0, sizeof(msg));  
    }
    else 
    {
        printf("All Jobs Completed (Total Time: %d)\n", current_time);
    }
    
    float total_tat = 0;
    float total_wt = 0;

    if (log == 1)
    {
        push_log("Scheduling Calculations (only completed jobs are tracker): \n");
    }
    else 
    {
        printf("\nScheduling Calculations (only completed jobs are tracker): \n");
    }

    int completed_jobs = 0; //track only completed jobs

    for (int i = 0; i < num_jobs; i++) 
    {
        if (jobs[i].s == STATUS_FAILED) 
        {
            if (log == 1)
            {
                sprintf(msg, "Job %d: FAILED, continuing\n", jobs[i].job_id);   
                push_log(msg);
                memset(msg, 0, sizeof(msg));
            }
            else 
            {
                printf("Job %d: FAILED, continuing\n", jobs[i].job_id);
            }
            continue;
        }

        int tat = jobs[i].completion_time - jobs[i].arrival_time;
        int wt = tat - jobs[i].burst_time; 

        total_tat += tat;
        total_wt += wt;
        completed_jobs++;

        if (log == 1)
        {
            sprintf(msg, "Job %d: Turnaround Time = %d, Waiting Time = %d\n", jobs[i].job_id, tat, wt);  
            push_log(msg);
            memset(msg, 0, sizeof(msg));
        }
        else 
        {
            printf("Job %d: Turnaround Time = %d, Waiting Time = %d\n", jobs[i].job_id, tat, wt);
        }   
    }

    if (completed_jobs > 0) 
    {
        if (log == 1)
        {
            sprintf(msg, "Average Turnaround Time: %.2f\n", total_tat / completed_jobs); 
            push_log(msg);
            memset(msg, 0, sizeof(msg));

            sprintf(msg, "Average Waiting Time: %.2f\n", total_wt / completed_jobs); 
            push_log(msg);
            memset(msg, 0, sizeof(msg));
        }
        else 
        {
            printf("\nAverage Turnaround Time: %.2f\n", total_tat / completed_jobs);
            printf("Average Waiting Time: %.2f\n", total_wt / completed_jobs);
        }
    }
}