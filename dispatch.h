#ifndef DISPATCH_H
#define DISPATCH_H

#include "structs.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>

void execute_worker(Job* job);
void dispatch_job_fcfs(Job* job);
void run_round_robin(Job jobs[], int num_jobs, int quantum);

#endif