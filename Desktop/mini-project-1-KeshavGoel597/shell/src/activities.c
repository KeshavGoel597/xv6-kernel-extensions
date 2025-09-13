#include "definition.h"
// static BgJob bgjobs[MAX_BGJOBS];

// Helper for qsort
static int cmp_bgjobs_by_cmd(const void *a, const void *b) {
    const BgJob *ja = *(const BgJob **)a;
    const BgJob *jb = *(const BgJob **)b;
    return strcmp(ja->command, jb->command);
}

void bgjobs_activities(void) {
    // Remove terminated jobs and collect active ones
    BgJob *active[MAX_BGJOBS];
    int n = 0;
    int status;
    for (int i = 0; i < MAX_BGJOBS; ++i) {
        if (bgjobs[i].active) {
            pid_t ret = waitpid(bgjobs[i].pid, &status, WNOHANG | WUNTRACED | WCONTINUED);
            if (ret == 0) {
                // Still running or stopped
                active[n++] = &bgjobs[i];
            } else if (ret == -1) {
                // Error, treat as terminated
                bgjobs[i].active = 0;
            } else {
                // Exited
                bgjobs[i].active = 0;
            }
        }
    }
    // Sort by command name
    qsort(active, n, sizeof(BgJob *), cmp_bgjobs_by_cmd);
    // Print
    for (int i = 0; i < n; ++i) {
        const BgJob *job = active[i];
        char state[16] = "";
        // Check state
        int st;
        pid_t ret = waitpid(job->pid, &st, WNOHANG | WUNTRACED | WCONTINUED);
        if (ret == 0) {
            // Still running or stopped
            if (kill(job->pid, 0) == 0) {
                // Check if stopped
                if (waitpid(job->pid, &st, WNOHANG | WUNTRACED) > 0 && WIFSTOPPED(st)) {
                    strcpy(state, "Stopped");
                } else {
                    strcpy(state, "Running");
                }
            } else {
                strcpy(state, "Exited");
            }
        } else if (ret == -1) {
            strcpy(state, "Exited");
        } else {
            strcpy(state, "Exited");
        }
        printf("[%d] : %s - %s\n", job->pid, job->command, state);
    }
}