#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "structs.h"

#define SCHED_MAX_JOBS     64
#define SCHED_MAX_SEGMENTS 1024
#define SCHED_IDLE_ID      (-1)   /* job_id used for CPU-idle timeline segments */

/* Per-job results. metrics[i] corresponds to jobs[i] of the input array. */
typedef struct {
    int      job_id;
    Priority priority;
    int      arrival_time;
    int      burst_time;
    int      first_start;      /* time the job first got the CPU */
    int      completion_time;
    int      turnaround_time;  /* completion - arrival */
    int      waiting_time;     /* turnaround - burst   */
} JobMetrics;

/* One contiguous run on the CPU: [start, end) */
typedef struct {
    int job_id;                /* SCHED_IDLE_ID if the CPU was idle */
    int start;
    int end;
} Segment;

typedef struct {
    const char *name;          /* "FCFS", "Round Robin", "Priority (non-preemptive)" */
    const char *tie_rule;      /* tie-breaking rule, printed with the output */
    int         quantum;       /* time quantum for RR, 0 otherwise */
    int         n_jobs;
    JobMetrics  metrics[SCHED_MAX_JOBS];
    int         n_segments;
    Segment     timeline[SCHED_MAX_SEGMENTS];
    double      avg_turnaround;
    double      avg_waiting;
} ScheduleResult;

/*
 * Each scheduler reads (never modifies) the input jobs and fills `out`.
 * Return 0 on success, -1 on invalid input (a message goes to stderr).
 * Validation: 1..SCHED_MAX_JOBS jobs, unique job_id, arrival >= 0, burst > 0,
 * quantum > 0 (RR only).
 */
int schedule_fcfs(const Job *jobs, int n, ScheduleResult *out);
int schedule_rr(const Job *jobs, int n, int quantum, ScheduleResult *out);
int schedule_priority(const Job *jobs, int n, ScheduleResult *out);

/* Prints the algorithm, tie rule/quantum, timeline, per-job table and averages. */
void print_schedule(const ScheduleResult *r);

/* Runs all three policies on the same job set and prints a comparison table.
   Returns 0 on success, -1 if any run fails. */
int print_comparison(const Job *jobs, int n, int quantum);

/* ------------------------------------------------------------------
 * FCFS job queue  (the ONLY scheduler API other modules use)
 *
 * Jobs normally arrive here through banker_admit() (banker.h), which only
 * passes on a job list the Banker's algorithm found safe.
 * The scheduler's sole responsibility is ordering jobs. It never forks,
 * runs or tracks workers: an external dispatcher decides WHEN to take a job
 * (e.g. after the Banker's safety check) and forks the worker itself.
 *
 * Order matches schedule_fcfs(): arrival time, then lower job ID.
 * RR and Priority above remain batch showcases only.
 *
 * Jobs are copied into an internal pool on enqueue and never move, so a
 * Job* returned by peek/dequeue stays valid until sched_init() is called
 * again. Callers may update the job's status field through the pointer
 * (the scheduler itself only sets STATUS_WAITING on enqueue).
 * Not thread-safe; use from the parent process only.
 * ------------------------------------------------------------------ */

/* Clear the queue and forget all jobs. Invalidates earlier Job pointers. */
void sched_init(void);

/* Copy a job into the queue. Returns 0 on success, -1 on invalid job
   (arrival < 0, burst <= 0), duplicate job_id, or full queue. */
int  sched_enqueue(const Job *job);

/* Enqueue a whole list atomically: either every job is queued or (on any
   invalid job, duplicate ID or lack of space) none is, and -1 is returned.
   This is what banker_admit() uses once a job list is found safe. */
int  sched_enqueue_list(const Job *jobs, int n);

/* Pointer to the next job WITHOUT removing it, or NULL if the queue is empty.
   Lets the dispatcher run the Banker's check first and leave the job queued
   if the request is unsafe. */
Job *sched_peek(void);

/* Remove the next job from the queue and return a pointer to it,
   or NULL if the queue is empty. */
Job *sched_dequeue(void);

/* Number of jobs still waiting in the queue. */
int  sched_pending(void);

#endif