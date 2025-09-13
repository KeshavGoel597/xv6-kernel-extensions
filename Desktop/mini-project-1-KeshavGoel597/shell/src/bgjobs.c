#include "definition.h"

BgJob bgjobs[MAX_BGJOBS];
static int next_job_number = 1;

void bgjobs_init(void) {
    for (int i = 0; i < MAX_BGJOBS; ++i) bgjobs[i].active = 0;
    next_job_number = 1;
}

int bgjobs_add(pid_t pid, const char *cmd) {
    for (int i = 0; i < MAX_BGJOBS; ++i) {
        if (!bgjobs[i].active) {
            bgjobs[i].job_number = next_job_number++;
            bgjobs[i].pid = pid;
            strncpy(bgjobs[i].command, cmd, MAX_INPUT_SIZE-1);
            bgjobs[i].command[MAX_INPUT_SIZE-1] = '\0';
            bgjobs[i].active = 1;
            return bgjobs[i].job_number;
        }
    }
    return -1;
}

void bgjobs_check_and_report(void) {
    int status;
    pid_t pid;
    for (int i = 0; i < MAX_BGJOBS; ++i) {
        if (bgjobs[i].active) {
            pid = waitpid(bgjobs[i].pid, &status, WNOHANG);
            if (pid > 0) {
                if (WIFEXITED(status)) {
                    printf("%s with pid %d exited normally\n", bgjobs[i].command, bgjobs[i].pid);
                } else {
                    printf("%s with pid %d exited abnormally\n", bgjobs[i].command, bgjobs[i].pid);
                }
                bgjobs[i].active = 0;
            }
        }
    }
}
