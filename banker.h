#ifndef BANKER_H
#define BANKER_H

#include "structs.h"
#include "scheduler.h"

#define BANKER_MAX_PROCS SCHED_MAX_JOBS

/* Return codes of banker_admit() (negative = error, see stderr) */
#define BANKER_ADMITTED  0   /* state safe: every job was passed to the scheduler */
#define BANKER_UNSAFE    1   /* state unsafe: nothing was enqueued */

/* Everything the algorithm saw and decided; used by banker_print(). */
typedef struct {
    int n;                                                  /* number of jobs */
    int job_id[BANKER_MAX_PROCS];
    int total[NUM_RESOURCES];                               /* RESOURCE_TOTAL */
    int available[NUM_RESOURCES];                           /* total - sum of allocations */
    int allocation[BANKER_MAX_PROCS][NUM_RESOURCES];
    int max[BANKER_MAX_PROCS][NUM_RESOURCES];
    int need[BANKER_MAX_PROCS][NUM_RESOURCES];              /* max - allocation */

    int safe;                                               /* 1 = safe, 0 = unsafe */
    int n_sequence;                                         /* number of jobs placed in the sequence */
    int sequence[BANKER_MAX_PROCS];                         /* job IDs in safe order */
    int order[BANKER_MAX_PROCS];                            /* same, as indices into job_id[] */
    int work_before[BANKER_MAX_PROCS][NUM_RESOURCES];       /* Work vector before each step */
    int n_stuck;
    int stuck[BANKER_MAX_PROCS];                            /* job IDs that could never finish (unsafe) */
} BankerResult;

/*
 * Banker's safety algorithm over a job list. Pure analysis: no queueing.
 *   Available = RESOURCE_TOTAL - sum of every job's `allocated`
 *   Need      = max_claim - allocated
 * Work = Available; repeatedly take the FIRST (lowest-index) unfinished job
 * whose Need <= Work and add its allocation to Work. Safe iff all jobs finish.
 * Returns 0 if the analysis ran (check out->safe), -1 on invalid input.
 * Validation: 1..BANKER_MAX_PROCS jobs, unique job IDs, no negative values,
 * allocated <= max_claim, max_claim <= RESOURCE_TOTAL, and the summed
 * allocations must not exceed RESOURCE_TOTAL.
 */
int banker_check(const Job *jobs, int n, BankerResult *out);

/*
 * Gatekeeper for the scheduler: runs banker_check() and, ONLY if the state is
 * safe, enqueues all jobs via sched_enqueue_list().
 * `out` may be NULL. Returns BANKER_ADMITTED, BANKER_UNSAFE (nothing
 * enqueued), or -1 on error (nothing enqueued). Does not call sched_init().
 */
int banker_admit(const Job *jobs, int n, BankerResult *out);

/* Prints Total/Available, Allocation/Maximum/Need per job, each step taken,
   and the verdict (safe sequence, or the jobs that cannot finish). */
void banker_print(const BankerResult *r);

void banker_print_buffer(const BankerResult *r, char *out);

#endif
