#ifndef DEFINITION_H
#define DEFINITION_H

#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include <dirent.h>
#include <sys/utsname.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <regex.h>   
#include <ctype.h> 
#include <dirent.h>
#include <sys/wait.h>
#include <errno.h>
#include <limits.h> 
#include <sys/types.h>
// Define constants
#define size 1024
#define MAX_INPUT_SIZE 4097
#define MAX_GROUPS 128 
#define PATH_MAX 4096
// Log history
#define LOG_MAX 15
// Declare global variables
extern char *prevdir;

typedef struct {
    int  count;                 // number of cmd_groups
    char *group[MAX_GROUPS];    // each cmd_group substring (trimmed)
    char sep[MAX_GROUPS];       // separator after group i: ';' or '&' (0 for last)
    int  trailing_amp;          // 1 if final optional '&' present
} ShellTop;



#ifndef MAX_BGJOBS
#define MAX_BGJOBS 64
#endif

#ifndef WCONTINUED
#define WCONTINUED 0
#endif

typedef struct {
    int job_number;
    pid_t pid;
    char command[MAX_INPUT_SIZE];
    int active;
} BgJob;


extern BgJob bgjobs[MAX_BGJOBS];


extern volatile pid_t fg_pgid;
void install_shell_signal_handlers(void);

// Declare function prototypes
void hopfunc(char *input, char *home);
int is_valid_shell_cmd(const char *s);
int split_shell_cmd(const char *s, ShellTop *out);
void free_shell_top(ShellTop *st);

// Validate a single cmd_group by splitting on '|' and checking atomics
int validate_cmd_group(const char *group);




int process_hop_commands(ShellTop *top, char *home);
int process_reveal_commands(ShellTop *top, char *home);
void revfunc(int show_hidden, int long_format, char *path, char *home);
int execute_standard_commands(ShellTop *top);
int is_reveal_atomic(const char *atomic_cmd);
int is_hop_atomic(const char *atomic_cmd);
int is_log_atomic(const char *atomic_cmd);

// Log history API
void log_init(void);
void log_shutdown(void);
void log_print(void);
void log_purge(void);
void log_add_if_allowed(const char *line, const ShellTop *top);
int shell_contains_log(const ShellTop *top);
// Returns: 0 = not a log command; 1 = handled (print/purge); 2 = execute, and fills exec_buf with command to execute
int handle_log_command(const ShellTop *top, char *exec_buf, size_t buf_size);


// In definition.h

char *dup_trim_range(const char *start, const char *end);
int parse_atomic_to_argv(const char *atomic_cmd, char ***argv, int *argc);
int builtin_cd(char **argv, int argc);
void free_argv(char **argv, int argc);
int execute_atomic_command(const char *atomic_cmd);


void fg_job(int job_number);
void bg_job(int job_number);


void pingfunc(pid_t pid, int sig);
// Background job management
void bgjobs_init(void);
int bgjobs_add(pid_t pid, const char *cmd);
void bgjobs_check_and_report(void);

void bgjobs_activities(void);
int execute_pipeline_group(const char *group);
#endif // DEFINITION_H

