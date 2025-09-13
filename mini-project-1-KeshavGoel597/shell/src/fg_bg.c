#include "definition.h" 
//######## LLM Generated Code Begins ##############
// Find job by job_number, or most recent if job_number==0
static BgJob* find_job(int job_number) {
    BgJob *last = NULL;
    for (int i = 0; i < MAX_BGJOBS; ++i) {
        if (bgjobs[i].active) {
            if (job_number == 0) {
                if (!last || bgjobs[i].job_number > last->job_number) last = &bgjobs[i];
            } else if (bgjobs[i].job_number == job_number) {
                return &bgjobs[i];
            }
        }
    }
    return (job_number == 0) ? last : NULL;
}

void fg_job(int job_number) {
    BgJob *job = find_job(job_number);
    if (!job) {
        printf("No such job\n");
        return;
    }
    printf("%s\n", job->command);
    // Send SIGCONT if stopped
    kill(job->pid, SIGCONT);
    // Give terminal to job
    tcsetpgrp(STDIN_FILENO, job->pid);
    int status;
    waitpid(job->pid, &status, WUNTRACED);
    // Restore terminal to shell
    tcsetpgrp(STDIN_FILENO, getpgrp());
    if (WIFSTOPPED(status)) {
        // Still stopped, keep in bgjobs
    } else {
        job->active = 0;
    }
}

void bg_job(int job_number) {
    BgJob *job = find_job(job_number);
    if (!job) {
        printf("No such job\n");
        return;
    }
    // Check if already running
    int status;
    pid_t ret = waitpid(job->pid, &status, WNOHANG | WUNTRACED);
    if (ret == 0) {
        // Still running or stopped
        if (kill(job->pid, 0) == 0) {
            if (waitpid(job->pid, &status, WNOHANG | WUNTRACED) > 0 && WIFSTOPPED(status)) {
                // Stopped, resume
                kill(job->pid, SIGCONT);
                printf("[%d] %s &\n", job->job_number, job->command);
            } else {
                printf("Job already running\n");
            }
        } else {
            printf("No such job\n");
            job->active = 0;
        }
    } else {
        printf("No such job\n");
        job->active = 0;
    }
}
//######## LLM Generated Code ends ##############