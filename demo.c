/* demo.c - quick test driver for scheduler.c and banker.c (not the final main.c) */
#include <stdio.h>
#include <string.h>
#include "structs.h"
#include "scheduler.h"
#include "banker.h"

static Job mk(int id, int arrival, int burst, Priority p, const int alloc[3], const int max[3])
{
    Job j;
    memset(&j, 0, sizeof j);
    j.job_id = id; j.arrival_time = arrival; j.burst_time = burst; j.p = p; j.s = STATUS_WAITING;
    memcpy(j.allocated, alloc, sizeof j.allocated);
    memcpy(j.max_claim, max, sizeof j.max_claim);
    return j;
}

/* Textbook-style safe state against capacities 9/12/15 */
static int safe_jobs(Job *j)
{
    int A[5][3] = {{0,1,0},{2,0,0},{3,0,2},{2,1,1},{0,0,2}};
    int X[5][3] = {{7,5,3},{3,2,2},{9,0,2},{2,2,2},{4,3,3}};
    int arrival[5] = {4, 0, 2, 1, 3};
    for (int i = 0; i < 5; i++) j[i] = mk(i + 1, arrival[i], 3 + i, PRIORITY_LOW, A[i], X[i]);
    return 5;
}

/* Deadlock pair: each job holds what the other still needs */
static int unsafe_jobs(Job *j)
{
    int a1[3] = {9,0,0}, m1[3] = {9,12,0}, a2[3] = {0,12,0}, m2[3] = {9,12,0};
    j[0] = mk(1, 0, 3, PRIORITY_LOW, a1, m1);
    j[1] = mk(2, 1, 3, PRIORITY_LOW, a2, m2);
    return 2;
}

static void drain_queue(void)
{
    printf("Dequeue order:");
    Job *p;
    while ((p = sched_dequeue()) != NULL) printf(" J%d", p->job_id);
    printf("\n");
}

static void usage(const char *prog)
{
    fprintf(stderr, "Usage: %s <sched | banker-safe | banker-unsafe | flow-safe | flow-unsafe>\n", prog);
}

int main(int argc, char **argv)
{
    if (argc != 2) { usage(argv[0]); return 1; }
    const char *cmd = argv[1];
    Job jobs[8];
    BankerResult br;
    int n;

    if (strcmp(cmd, "sched") == 0) {
        int z[3] = {0,0,0};
        /* includes an idle gap (J4 arrives late) and tied arrivals (J2, J5) */
        jobs[0] = mk(1, 0, 5, PRIORITY_LOW, z, z);
        jobs[1] = mk(2, 1, 3, PRIORITY_LOW, z, z);
        jobs[2] = mk(3, 2, 8, PRIORITY_HIGH, z, z);
        jobs[3] = mk(4, 20, 2, PRIORITY_HIGH, z, z);
        jobs[4] = mk(5, 1, 4, PRIORITY_MEDIUM, z, z);
        n = 5;
        ScheduleResult r;
        if (schedule_fcfs(jobs, n, &r) == 0) print_schedule(&r);
        if (schedule_rr(jobs, n, 3, &r) == 0) print_schedule(&r);
        if (schedule_priority(jobs, n, &r) == 0) print_schedule(&r);
        return print_comparison(jobs, n, 3) == 0 ? 0 : 1;
    }
    if (strcmp(cmd, "banker-safe") == 0 || strcmp(cmd, "banker-unsafe") == 0) {
        n = strcmp(cmd, "banker-safe") == 0 ? safe_jobs(jobs) : unsafe_jobs(jobs);
        if (banker_check(jobs, n, &br) != 0) return 1;
        banker_print(&br);
        return 0;
    }
    if (strcmp(cmd, "flow-safe") == 0 || strcmp(cmd, "flow-unsafe") == 0) {
        n = strcmp(cmd, "flow-safe") == 0 ? safe_jobs(jobs) : unsafe_jobs(jobs);
        sched_init();
        int rc = banker_admit(jobs, n, &br);
        banker_print(&br);
        printf("\nbanker_admit returned %d (%s)\n", rc,
               rc == BANKER_ADMITTED ? "admitted" : rc == BANKER_UNSAFE ? "rejected as unsafe" : "error");
        printf("Jobs waiting in scheduler queue: %d\n", sched_pending());
        drain_queue();
        return rc < 0 ? 1 : 0;
    }

    fprintf(stderr, "Unknown command '%s'\n", cmd);
    usage(argv[0]);
    return 1;
}