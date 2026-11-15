#include <stdio.h>
#include <string.h>
#include "scheduler.h"

/* Tie-breaking rules (printed in the output) */
#define TIE_FCFS "equal arrival time -> lower job ID first"
#define TIE_RR   "jobs arriving at the same instant a quantum expires are queued before the preempted job; equal arrivals -> lower job ID first"
#define TIE_PRIO "higher priority first (HIGH > MEDIUM > LOW); tie -> earlier arrival; tie -> lower job ID"

/* ---------- helpers ---------- */

static const char *priority_str(Priority p)
{
    switch (p) {
        case PRIORITY_LOW:    return "LOW";
        case PRIORITY_MEDIUM: return "MEDIUM";
        case PRIORITY_HIGH:   return "HIGH";
        default:              return "?";
    }
}

static int validate(const Job *jobs, int n)
{
    if (jobs == NULL || n < 1 || n > SCHED_MAX_JOBS) {
        fprintf(stderr, "scheduler: job count must be between 1 and %d\n", SCHED_MAX_JOBS);
        return -1;
    }
    for (int i = 0; i < n; i++) {
        if (jobs[i].arrival_time < 0) {
            fprintf(stderr, "scheduler: job %d has negative arrival time\n", jobs[i].job_id);
            return -1;
        }
        if (jobs[i].burst_time <= 0) {
            fprintf(stderr, "scheduler: job %d must have a burst time > 0\n", jobs[i].job_id);
            return -1;
        }
        for (int j = 0; j < i; j++) {
            if (jobs[j].job_id == jobs[i].job_id) {
                fprintf(stderr, "scheduler: duplicate job ID %d\n", jobs[i].job_id);
                return -1;
            }
        }
    }
    return 0;
}

/* order[] = indices of jobs sorted by (arrival, job_id). Insertion sort: n is small. */
static void sort_by_arrival(const Job *jobs, int n, int *order)
{
    for (int i = 0; i < n; i++) order[i] = i;
    for (int i = 1; i < n; i++) {
        int key = order[i], j = i - 1;
        while (j >= 0 &&
               (jobs[order[j]].arrival_time > jobs[key].arrival_time ||
                (jobs[order[j]].arrival_time == jobs[key].arrival_time &&
                 jobs[order[j]].job_id > jobs[key].job_id))) {
            order[j + 1] = order[j];
            j--;
        }
        order[j + 1] = key;
    }
}

static void init_result(ScheduleResult *r, const char *name, const char *tie,
                        int quantum, const Job *jobs, int n)
{
    memset(r, 0, sizeof *r);
    r->name = name;
    r->tie_rule = tie;
    r->quantum = quantum;
    r->n_jobs = n;
    for (int i = 0; i < n; i++) {
        r->metrics[i].job_id = jobs[i].job_id;
        r->metrics[i].priority = jobs[i].p;
        r->metrics[i].arrival_time = jobs[i].arrival_time;
        r->metrics[i].burst_time = jobs[i].burst_time;
        r->metrics[i].first_start = -1;
    }
}

/* Appends [start,end) to the timeline, merging with the previous segment if contiguous. */
static int add_segment(ScheduleResult *r, int job_id, int start, int end)
{
    if (end <= start) return 0;
    if (r->n_segments > 0) {
        Segment *last = &r->timeline[r->n_segments - 1];
        if (last->job_id == job_id && last->end == start) {
            last->end = end;
            return 0;
        }
    }
    if (r->n_segments >= SCHED_MAX_SEGMENTS) {
        fprintf(stderr, "scheduler: timeline too long (max %d segments)\n", SCHED_MAX_SEGMENTS);
        return -1;
    }
    r->timeline[r->n_segments++] = (Segment){ job_id, start, end };
    return 0;
}

static void finalize(ScheduleResult *r)
{
    double tat = 0, wt = 0;
    for (int i = 0; i < r->n_jobs; i++) {
        JobMetrics *m = &r->metrics[i];
        m->turnaround_time = m->completion_time - m->arrival_time;
        m->waiting_time = m->turnaround_time - m->burst_time;
        tat += m->turnaround_time;
        wt += m->waiting_time;
    }
    r->avg_turnaround = tat / r->n_jobs;
    r->avg_waiting = wt / r->n_jobs;
}

/* ---------- FCFS ---------- */

int schedule_fcfs(const Job *jobs, int n, ScheduleResult *out)
{
    if (validate(jobs, n) != 0 || out == NULL) return -1;

    int order[SCHED_MAX_JOBS];
    sort_by_arrival(jobs, n, order);
    init_result(out, "FCFS", TIE_FCFS, 0, jobs, n);

    int t = 0;
    for (int k = 0; k < n; k++) {
        int i = order[k];
        if (t < jobs[i].arrival_time) {                 /* CPU idle until arrival */
            if (add_segment(out, SCHED_IDLE_ID, t, jobs[i].arrival_time) != 0) return -1;
            t = jobs[i].arrival_time;
        }
        out->metrics[i].first_start = t;
        if (add_segment(out, jobs[i].job_id, t, t + jobs[i].burst_time) != 0) return -1;
        t += jobs[i].burst_time;
        out->metrics[i].completion_time = t;
    }
    finalize(out);
    return 0;
}

/* ---------- Round Robin ---------- */

int schedule_rr(const Job *jobs, int n, int quantum, ScheduleResult *out)
{
    if (validate(jobs, n) != 0 || out == NULL) return -1;
    if (quantum <= 0) {
        fprintf(stderr, "scheduler: time quantum must be > 0\n");
        return -1;
    }

    int order[SCHED_MAX_JOBS], remaining[SCHED_MAX_JOBS];
    int queue[SCHED_MAX_JOBS], head = 0, count = 0;  /* circular ready queue; count <= n */
    sort_by_arrival(jobs, n, order);
    init_result(out, "Round Robin", TIE_RR, quantum, jobs, n);
    for (int i = 0; i < n; i++) remaining[i] = jobs[i].burst_time;

    int t = 0, next = 0, done = 0;
    while (done < n) {
        /* admit everything that has arrived by now */
        while (next < n && jobs[order[next]].arrival_time <= t) {
            queue[(head + count++) % SCHED_MAX_JOBS] = order[next++];
        }
        if (count == 0) {                               /* idle until next arrival */
            int na = jobs[order[next]].arrival_time;
            if (add_segment(out, SCHED_IDLE_ID, t, na) != 0) return -1;
            t = na;
            continue;
        }

        int i = queue[head];
        head = (head + 1) % SCHED_MAX_JOBS;
        count--;

        if (out->metrics[i].first_start < 0) out->metrics[i].first_start = t;
        int slice = remaining[i] < quantum ? remaining[i] : quantum;
        if (add_segment(out, jobs[i].job_id, t, t + slice) != 0) return -1;
        t += slice;
        remaining[i] -= slice;

        /* new arrivals go in BEFORE the preempted job is re-queued */
        while (next < n && jobs[order[next]].arrival_time <= t) {
            queue[(head + count++) % SCHED_MAX_JOBS] = order[next++];
        }
        if (remaining[i] > 0) {
            queue[(head + count++) % SCHED_MAX_JOBS] = i;
        } else {
            out->metrics[i].completion_time = t;
            done++;
        }
    }
    finalize(out);
    return 0;
}

/* ---------- Non-preemptive Priority ---------- */

/* true if job a should be picked over job b */
static int better(const Job *a, const Job *b)
{
    if (a->p != b->p) return a->p > b->p;               /* HIGH has the largest enum value */
    if (a->arrival_time != b->arrival_time) return a->arrival_time < b->arrival_time;
    return a->job_id < b->job_id;
}

int schedule_priority(const Job *jobs, int n, ScheduleResult *out)
{
    if (validate(jobs, n) != 0 || out == NULL) return -1;

    int finished[SCHED_MAX_JOBS] = {0};
    init_result(out, "Priority (non-preemptive)", TIE_PRIO, 0, jobs, n);

    int t = 0;
    for (int done = 0; done < n; ) {
        int best = -1, earliest = -1;
        for (int i = 0; i < n; i++) {
            if (finished[i]) continue;
            if (earliest < 0 || jobs[i].arrival_time < jobs[earliest].arrival_time) earliest = i;
            if (jobs[i].arrival_time <= t && (best < 0 || better(&jobs[i], &jobs[best])))
                best = i;
        }
        if (best < 0) {                                  /* nobody ready: idle */
            int na = jobs[earliest].arrival_time;
            if (add_segment(out, SCHED_IDLE_ID, t, na) != 0) return -1;
            t = na;
            continue;
        }
        out->metrics[best].first_start = t;
        if (add_segment(out, jobs[best].job_id, t, t + jobs[best].burst_time) != 0) return -1;
        t += jobs[best].burst_time;
        out->metrics[best].completion_time = t;
        finished[best] = 1;
        done++;
    }
    finalize(out);
    return 0;
}

/* ---------- Output ---------- */

void print_schedule(const ScheduleResult *r)
{
    printf("\n=== %s ===\n", r->name);
    if (r->quantum > 0) printf("Time quantum : %d\n", r->quantum);
    printf("Tie-breaking : %s\n", r->tie_rule);

    printf("\nTimeline:\n");
    for (int i = 0; i < r->n_segments; i++) {
        const Segment *s = &r->timeline[i];
        if (s->job_id == SCHED_IDLE_ID) printf("| IDLE (%d-%d) ", s->start, s->end);
        else                            printf("| J%d (%d-%d) ", s->job_id, s->start, s->end);
    }
    printf("|\n\n");

    printf("%-5s %-8s %-8s %-6s %-6s %-11s %-11s %-8s\n",
           "Job", "Priority", "Arrival", "Burst", "Start", "Completion", "Turnaround", "Waiting");
    for (int i = 0; i < r->n_jobs; i++) {
        const JobMetrics *m = &r->metrics[i];
        printf("%-5d %-8s %-8d %-6d %-6d %-11d %-11d %-8d\n",
               m->job_id, priority_str(m->priority), m->arrival_time, m->burst_time,
               m->first_start, m->completion_time, m->turnaround_time, m->waiting_time);
    }
    printf("\nAverage turnaround time: %.2f\n", r->avg_turnaround);
    printf("Average waiting time   : %.2f\n", r->avg_waiting);
}

int print_comparison(const Job *jobs, int n, int quantum)
{
    ScheduleResult res[3];
    if (schedule_fcfs(jobs, n, &res[0]) != 0) return -1;
    if (schedule_rr(jobs, n, quantum, &res[1]) != 0) return -1;
    if (schedule_priority(jobs, n, &res[2]) != 0) return -1;

    printf("\n=== Scheduling comparison (same %d jobs, RR quantum = %d) ===\n", n, quantum);
    printf("%-28s %-16s %-16s\n", "Algorithm", "Avg Turnaround", "Avg Waiting");
    int best = 0;
    for (int i = 0; i < 3; i++) {
        printf("%-28s %-16.2f %-16.2f\n", res[i].name, res[i].avg_turnaround, res[i].avg_waiting);
        if (res[i].avg_waiting < res[best].avg_waiting) best = i;
    }
    printf("Lowest average waiting time: %s\n", res[best].name);
    return 0;
}

/* ---------- FCFS job queue ---------- */

/* Jobs live in g_pool and never move; g_queue holds pool indices of the
   waiting jobs, sorted by (arrival, job_id). */
static Job g_pool[SCHED_MAX_JOBS];
static int g_n_pool = 0;              /* jobs enqueued since sched_init() */
static int g_queue[SCHED_MAX_JOBS];
static int g_count = 0;               /* jobs currently waiting */

void sched_init(void)
{
    g_n_pool = 0;
    g_count = 0;
}

/* Field checks shared by sched_enqueue and sched_enqueue_list. */
static int job_valid(const Job *job)
{
    if (job == NULL) {
        fprintf(stderr, "scheduler: NULL job\n");
        return -1;
    }
    if (job->arrival_time < 0 || job->burst_time <= 0) {
        fprintf(stderr, "scheduler: job %d rejected (arrival must be >= 0, burst > 0)\n", job->job_id);
        return -1;
    }
    return 0;
}

static int in_pool(int job_id)
{
    for (int i = 0; i < g_n_pool; i++)
        if (g_pool[i].job_id == job_id) return 1;
    return 0;
}

int sched_enqueue(const Job *job)
{
    if (job_valid(job) != 0) return -1;
    if (in_pool(job->job_id)) {
        fprintf(stderr, "scheduler: duplicate job ID %d\n", job->job_id);
        return -1;
    }
    if (g_n_pool >= SCHED_MAX_JOBS) {
        fprintf(stderr, "scheduler: queue full (max %d jobs)\n", SCHED_MAX_JOBS);
        return -1;
    }

    int idx = g_n_pool++;
    g_pool[idx] = *job;
    g_pool[idx].s = STATUS_WAITING;

    /* sorted insert: after every job that arrives earlier or ties with a lower ID */
    int pos = g_count;
    while (pos > 0 &&
           (g_pool[g_queue[pos - 1]].arrival_time > job->arrival_time ||
            (g_pool[g_queue[pos - 1]].arrival_time == job->arrival_time &&
             g_pool[g_queue[pos - 1]].job_id > job->job_id))) {
        g_queue[pos] = g_queue[pos - 1];
        pos--;
    }
    g_queue[pos] = idx;
    g_count++;
    return 0;
}

int sched_enqueue_list(const Job *jobs, int n)
{
    if (jobs == NULL || n < 1) {
        fprintf(stderr, "scheduler: empty job list\n");
        return -1;
    }
    if (g_n_pool + n > SCHED_MAX_JOBS) {
        fprintf(stderr, "scheduler: not enough space for %d jobs (max %d)\n", n, SCHED_MAX_JOBS);
        return -1;
    }
    /* validate everything first so the enqueue below cannot fail half-way */
    for (int i = 0; i < n; i++) {
        if (job_valid(&jobs[i]) != 0) return -1;
        int dup = in_pool(jobs[i].job_id);
        for (int j = 0; j < i && !dup; j++) dup = (jobs[j].job_id == jobs[i].job_id);
        if (dup) {
            fprintf(stderr, "scheduler: duplicate job ID %d\n", jobs[i].job_id);
            return -1;
        }
    }
    for (int i = 0; i < n; i++) sched_enqueue(&jobs[i]);
    return 0;
}

Job *sched_peek(void)
{
    return g_count == 0 ? NULL : &g_pool[g_queue[0]];
}

Job *sched_dequeue(void)
{
    if (g_count == 0) return NULL;
    Job *j = &g_pool[g_queue[0]];
    memmove(&g_queue[0], &g_queue[1], (size_t)(g_count - 1) * sizeof(int));
    g_count--;
    return j;
}

int sched_pending(void)
{
    return g_count;
}
