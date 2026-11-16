#ifndef DISPATCH_H
#define DISPATCH_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <sys/resource.h>
#include <time.h>
#include <string.h>

#include "structs.h"
#include "logger_pthread.h"

void execute_worker(Job* job, int log);
void dispatch_job_fcfs(Job* job, int log);
void run_round_robin(Job jobs[], int num_jobs, int quantum, int log);

#endif