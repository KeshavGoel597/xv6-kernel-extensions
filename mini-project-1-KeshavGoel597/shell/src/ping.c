#include "definition.h"
//######## LLM Generated Code Begins ##############
void pingfunc(pid_t pid, int sig) {
    int actual_signal = sig % 32;
    if (kill(pid, 0) == -1) {
        printf("No such process found\n");
        return;
    }
    if (kill(pid, actual_signal) == 0) {
        printf("Sent signal %d to process with pid %d\n", sig, pid);
    } else {
        printf("No such process found\n");
    }
}
//######## LLM Generated Code ends ##############
