#include <stdio.h>
#include "dispatch.h"
#include "structs.h"
#include "logger.h"


int main() {
    
    Job jobs[3] = {
        { .job_id = 1, .arrival_time = 0, .burst_time = 4, .remaining_time = 4, .pid = 0, .s = STATUS_WAITING },
        { .job_id = 2, .arrival_time = 1, .burst_time = 5, .remaining_time = 5, .pid = 0, .s = STATUS_WAITING },
        { .job_id = 3, .arrival_time = 5, .burst_time = 2, .remaining_time = 2, .pid = 0, .s = STATUS_WAITING }
    };

    run_round_robin(jobs, 3, 2);

    test_logger();

    return 0;
}