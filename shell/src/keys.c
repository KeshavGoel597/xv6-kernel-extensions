#include <signal.h>
#include <sys/types.h>
#ifndef SA_RESTART
#define SA_RESTART 0
#endif
#include "definition.h"

volatile pid_t fg_pgid = -1;

static void sigint_handler(int signo) {
	if (fg_pgid > 0) {
		kill(-fg_pgid, SIGINT);
	}
	// Do not exit shell
}

// Helper to find job by pid and mark as stopped, or add if not present
static void mark_job_stopped(pid_t pgid) {
	// Find job by pid
	for (int i = 0; i < MAX_BGJOBS; ++i) {
		if (bgjobs[i].active && bgjobs[i].pid == pgid) {
			// Already in bgjobs, just print stopped message
			printf("[%d] Stopped %s\n", bgjobs[i].job_number, bgjobs[i].command);
			fflush(stdout);
			return;
		}
	}
	// Not found, add to bgjobs (best effort: use pgid as pid, command unknown)
	int jobnum = bgjobs_add(pgid, "[stopped process]");
	printf("[%d] Stopped %s\n", jobnum, "[stopped process]");
	fflush(stdout);
}

static void sigtstp_handler(int signo) {
	if (fg_pgid > 0) {
		kill(-fg_pgid, SIGTSTP);
		// Mark job as stopped in bgjobs and print message
		mark_job_stopped(fg_pgid);
	}
	// Do not stop shell
}

void install_shell_signal_handlers(void) {
	struct sigaction sa_int, sa_tstp;
	sa_int.sa_handler = sigint_handler;
	sigemptyset(&sa_int.sa_mask);
	sa_int.sa_flags = SA_RESTART;
	sigaction(SIGINT, &sa_int, NULL);

	sa_tstp.sa_handler = sigtstp_handler;
	sigemptyset(&sa_tstp.sa_mask);
	sa_tstp.sa_flags = SA_RESTART;
	sigaction(SIGTSTP, &sa_tstp, NULL);
}
