#include <stdio.h>
#include <string.h>
#include "banker.h"

#define M NUM_RESOURCES

/* ---------- helpers ---------- */

static int validate(const Job *jobs, int n)
{
    if (jobs == NULL) {
        fprintf(stderr, "banker: NULL job list\n");
        return -1;
    }
    if (n < 1 || n > BANKER_MAX_PROCS) {
        fprintf(stderr, "banker: job count must be between 1 and %d\n", BANKER_MAX_PROCS);
        return -1;
    }
    int held[M] = {0};
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < i; j++) {
            if (jobs[j].job_id == jobs[i].job_id) {
                fprintf(stderr, "banker: duplicate job ID %d\n", jobs[i].job_id);
                return -1;
            }
        }
        for (int r = 0; r < M; r++) {
            if (jobs[i].max_claim[r] < 0 || jobs[i].allocated[r] < 0) {
                fprintf(stderr, "banker: job %d has a negative value (R%d)\n", jobs[i].job_id, r);
                return -1;
            }
            if (jobs[i].allocated[r] > jobs[i].max_claim[r]) {
                fprintf(stderr, "banker: job %d allocation exceeds its maximum (R%d)\n", jobs[i].job_id, r);
                return -1;
            }
            if (jobs[i].max_claim[r] > RESOURCE_TOTAL[r]) {
                fprintf(stderr, "banker: job %d maximum exceeds system capacity %d (R%d)\n",
                        jobs[i].job_id, RESOURCE_TOTAL[r], r);
                return -1;
            }
            held[r] += jobs[i].allocated[r];
        }
    }
    for (int r = 0; r < M; r++) {
        if (held[r] > RESOURCE_TOTAL[r]) {
            fprintf(stderr, "banker: allocations of R%d (%d) exceed system capacity %d\n",
                    r, held[r], RESOURCE_TOTAL[r]);
            return -1;
        }
    }
    return 0;
}

static int need_fits(const int *need, const int *work)
{
    for (int r = 0; r < M; r++)
        if (need[r] > work[r]) return 0;
    return 1;
}

/* Formats a resource vector as "[a b c]" into buf. */
static const char *vec_str(char *buf, size_t size, const int *v)
{
    size_t len = 0;
    len += (size_t)snprintf(buf + len, size - len, "[");
    for (int r = 0; r < M; r++)
        len += (size_t)snprintf(buf + len, size - len, r ? " %d" : "%d", v[r]);
    snprintf(buf + len, size - len, "]");
    return buf;
}

/* ---------- safety algorithm ---------- */

int banker_check(const Job *jobs, int n, BankerResult *out)
{
    if (out == NULL || validate(jobs, n) != 0) return -1;

    memset(out, 0, sizeof *out);
    out->n = n;
    for (int r = 0; r < M; r++) {
        out->total[r] = RESOURCE_TOTAL[r];
        out->available[r] = RESOURCE_TOTAL[r];
    }
    for (int i = 0; i < n; i++) {
        out->job_id[i] = jobs[i].job_id;
        for (int r = 0; r < M; r++) {
            out->max[i][r] = jobs[i].max_claim[r];
            out->allocation[i][r] = jobs[i].allocated[r];
            out->need[i][r] = jobs[i].max_claim[r] - jobs[i].allocated[r];   /* Need = Max - Allocation */
            out->available[r] -= jobs[i].allocated[r];                       /* Available = Total - held */
        }
    }

    int work[M], finished[BANKER_MAX_PROCS] = {0};
    memcpy(work, out->available, sizeof work);

    int k = 0;
    while (k < n) {
        int pick = -1;
        for (int i = 0; i < n; i++) {                   /* first unfinished job that fits */
            if (!finished[i] && need_fits(out->need[i], work)) { pick = i; break; }
        }
        if (pick < 0) break;                            /* nobody can proceed */

        memcpy(out->work_before[k], work, sizeof work);
        for (int r = 0; r < M; r++) work[r] += out->allocation[pick][r];   /* job finishes, releases all */
        finished[pick] = 1;
        out->order[k] = pick;
        out->sequence[k] = out->job_id[pick];
        k++;
    }

    out->n_sequence = k;
    out->safe = (k == n);
    for (int i = 0; i < n; i++)
        if (!finished[i]) out->stuck[out->n_stuck++] = out->job_id[i];
    return 0;
}

/* ---------- integration with the scheduler ---------- */

int banker_admit(const Job *jobs, int n, BankerResult *out)
{
    BankerResult local;
    BankerResult *r = out ? out : &local;

    if (banker_check(jobs, n, r) != 0) return -1;
    if (!r->safe) return BANKER_UNSAFE;                 /* scheduler never sees an unsafe list */
    if (sched_enqueue_list(jobs, n) != 0) return -1;
    return BANKER_ADMITTED;
}

/* ---------- output ---------- */

void banker_print(const BankerResult *r)
{
    char a[32], b[32], c[32];

    printf("\n=== Banker's safety analysis ===\n");
    printf("Total    : %s\n", vec_str(a, sizeof a, r->total));
    printf("Available: %s  (Total - sum of Allocation)\n", vec_str(a, sizeof a, r->available));

    printf("\n%-6s %-16s %-16s %-16s\n", "Job", "Allocation", "Maximum", "Need (Max-Alloc)");
    for (int i = 0; i < r->n; i++) {
        char name[16];
        snprintf(name, sizeof name, "J%d", r->job_id[i]);
        printf("%-6s %-16s %-16s %-16s\n", name,
               vec_str(a, sizeof a, r->allocation[i]),
               vec_str(b, sizeof b, r->max[i]),
               vec_str(c, sizeof c, r->need[i]));
    }

    printf("\nSteps (lowest-index job whose Need <= Work goes first):\n");
    for (int k = 0; k < r->n_sequence; k++) {
        int i = r->order[k], after[M];
        for (int x = 0; x < M; x++) after[x] = r->work_before[k][x] + r->allocation[i][x];
        printf("  %d. J%d: Need %s <= Work %s -> finishes, Work becomes %s\n",
               k + 1, r->job_id[i],
               vec_str(a, sizeof a, r->need[i]),
               vec_str(b, sizeof b, r->work_before[k]),
               vec_str(c, sizeof c, after));
    }

    if (r->safe) {
        printf("\nResult: SAFE state. Safe sequence: ");
        for (int k = 0; k < r->n_sequence; k++) printf(k ? " -> J%d" : "J%d", r->sequence[k]);
        printf("\n");
    } else {
        printf("\nResult: UNSAFE state. The remaining jobs cannot get their Need met; stuck jobs: ");
        for (int k = 0; k < r->n_stuck; k++) printf(k ? ", J%d" : "J%d", r->stuck[k]);
        printf("\n");
    }
}
