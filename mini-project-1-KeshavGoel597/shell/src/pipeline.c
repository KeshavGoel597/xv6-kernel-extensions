//ISSUE:echo    hello    |   tr -s ' ' '_' not working
#include "definition.h"
//######## LLM Generated Code Begins ##############
// Execute a pipeline like: atomic1 | atomic2 | ... | atomicN
// Returns 1 on successful orchestration (waited for all), 0 on immediate failure
int execute_pipeline_group(const char *group) {
    // 1) Split group by '|'
    const char *beg = group;
    while (*beg && isspace((unsigned char)*beg)) beg++;
    const char *end = group + strlen(group);
    while (end > beg && isspace((unsigned char)end[-1])) end--;

    // Count stages
    int stages_cap = 16;
    int stages = 0;
    char **atoms = (char **)malloc(stages_cap * sizeof(char *));
    if (!atoms) return 0;
    const char *seg = beg;
    for (const char *p = beg; p < end; ++p) {
        if (*p == '|') {
            char *a = dup_trim_range(seg, p);
            if (!a || a[0] == '\0') { free(a); goto fail_split; }
            if (stages >= stages_cap) {
                stages_cap *= 2;
                char **na = (char **)realloc(atoms, stages_cap * sizeof(char *));
                if (!na) { free(a); goto fail_split; }
                atoms = na;
            }
            atoms[stages++] = a;
            seg = p + 1;
        }
    }
    {
        char *a = dup_trim_range(seg, end);
        if (!a || a[0] == '\0') { free(a); goto fail_split; }
        if (stages >= stages_cap) {
            stages_cap *= 2;
            char **na = (char **)realloc(atoms, stages_cap * sizeof(char *));
            if (!na) { free(a); goto fail_split; }
            atoms = na;
        }
        atoms[stages++] = a;
    }

    if (stages == 0) goto fail_split;

    // 2) Create pipes
    int (*pipes)[2] = NULL;
    if (stages > 1) {
        pipes = (int (*)[2])malloc(sizeof(int[2]) * (size_t)(stages - 1));
        if (!pipes) goto fail_split;
        for (int i = 0; i < stages - 1; ++i) {
            if (pipe(pipes[i]) < 0) {
                perror("pipe");
                // close previously created
                for (int k = 0; k < i; ++k) { close(pipes[k][0]); close(pipes[k][1]); }
                free(pipes);
                goto fail_split;
            }
        }
    }

    // 3) Fork children
    pid_t *pids = (pid_t *)malloc(sizeof(pid_t) * (size_t)stages);
    if (!pids) {
        if (pipes) {
            for (int i = 0; i < stages - 1; ++i) { close(pipes[i][0]); close(pipes[i][1]); }
            free(pipes);
        }
        goto fail_split;
    }

    pid_t pgid = 0;
    for (int i = 0; i < stages; ++i) {
        pids[i] = fork();
        if (pids[i] < 0) {
            perror("fork");
            // Parent: still attempt to fork remaining stages? Requirement suggests continue running remaining commands.
            // We'll mark this pid as -1 and continue; later we'll wait only valid pids.
            pids[i] = -1;
            continue;
        }
        if (pids[i] == 0) {
            // Child: set process group (all children in same process group)
            setpgid(0, 0);  // First child creates new process group, others join it
            // Don't set terminal control in child - let parent handle it
            // Child
            // Set up default pipe redirections
            if (stages > 1) {
                if (i > 0) {
                    if (dup2(pipes[i-1][0], STDIN_FILENO) < 0) { perror("dup2"); _exit(1); }
                }
                if (i < stages - 1) {
                    if (dup2(pipes[i][1], STDOUT_FILENO) < 0) { perror("dup2"); _exit(1); }
                }
                // Close all pipe fds in child
                for (int k = 0; k < stages - 1; ++k) { close(pipes[k][0]); close(pipes[k][1]); }
            }

            // Parse redirections within this atomic and override defaults if present
            char *infile = NULL; 
            char *outfile = NULL; 
            int out_append = 0;
            {
                const char *p = atoms[i];
                while (*p) {
                    if (*p == '<') {
                        p++;
                        while (*p && isspace((unsigned char)*p)) p++;
                        const char *start = p;
                        while (*p && !isspace((unsigned char)*p)
                               && *p != '|' && *p != '&' && *p != '>' && *p != '<' && *p != ';') {
                            p++;
                        }
                        size_t n = (size_t)(p - start);
                        if (n > 0) {
                            char *tmp = (char *)malloc(n + 1);
                            if (!tmp) _exit(1);
                            memcpy(tmp, start, n);
                            tmp[n] = '\0';
                            free(infile);
                            infile = tmp;
                        }
                        continue;
                    } else if (*p == '>') {
                        int append = 0;
                        if (*(p + 1) == '>') { append = 1; p++; }
                        p++;
                        while (*p && isspace((unsigned char)*p)) p++;
                        const char *start = p;
                        while (*p && !isspace((unsigned char)*p)
                               && *p != '|' && *p != '&' && *p != '>' && *p != '<' && *p != ';') {
                            p++;
                        }
                        size_t n = (size_t)(p - start);
                        if (n > 0) {
                            char *tmp = (char *)malloc(n + 1);
                            if (!tmp) _exit(1);
                            memcpy(tmp, start, n);
                            tmp[n] = '\0';
                            free(outfile);
                            outfile = tmp;
                            out_append = append;
                        }
                        continue;
                    }
                    p++;
                }
            }

            int fd_in = -1, fd_out = -1;
            if (infile) {
                fd_in = open(infile, O_RDONLY);
                if (fd_in < 0) {
                    printf("No such file or directory\n");
                    _exit(1);
                }
                if (dup2(fd_in, STDIN_FILENO) < 0) { perror("dup2"); _exit(1); }
            }
            if (outfile) {
                int flags = O_WRONLY | O_CREAT | (out_append ? O_APPEND : O_TRUNC);
                fd_out = open(outfile, flags, 0666);
                if (fd_out < 0) { printf("Unable to create file for writing\n"); fflush(stdout); _exit(1); }
                if (dup2(fd_out, STDOUT_FILENO) < 0) { perror("dup2"); _exit(1); }
            }
            if (fd_in >= 0) close(fd_in);
            if (fd_out >= 0) close(fd_out);

            // Build argv and execute
            char **argv = NULL; int argc = 0;
            if (!parse_atomic_to_argv(atoms[i], &argv, &argc)) {
                _exit(1);
            }

            // Builtin: cd (runs in child, does not affect parent)
            if (argc > 0 && strcmp(argv[0], "cd") == 0) {
                int ok = builtin_cd(argv, argc);
                free_argv(argv, argc);
                _exit(ok ? 0 : 1);
            }
            
            // Builtin: reveal (runs in child)
            if (argc > 0 && strcmp(argv[0], "reveal") == 0) {
                // Parse reveal-specific arguments
                int show_hidden = 0, long_format = 0;
                char *path = NULL;
                
                // Parse flags and path
                for (int j = 1; j < argc; j++) {
                    if (argv[j][0] == '-' && argv[j][1] != '\0') {
                        // This is a flag (like -a or -l)
                        for (int k = 1; argv[j][k]; k++) {
                            if (argv[j][k] == 'a') show_hidden = 1;
                            else if (argv[j][k] == 'l') long_format = 1;
                            else {
                                printf("reveal: Invalid Syntax!\n");
                                fflush(stdout);
                                free_argv(argv, argc);
                                _exit(1);
                            }
                        }
                    } else {
                        // This is a path argument (including "-")
                        if (path) {
                            // Multiple paths - invalid
                            printf("reveal: Invalid Syntax!\n");
                            fflush(stdout);
                            free(path);
                            free_argv(argv, argc);
                            _exit(1);
                        }
                        path = strdup(argv[j]);
                    }
                }
                
                // Get home directory
                char *home = getenv("HOME");
                if (!home) home = "/";
                
                revfunc(show_hidden, long_format, path, home);
                
                free(path);
                free_argv(argv, argc);
                _exit(0);
            }

            execvp(argv[0], argv);
            printf("Command not found!\n");
            fflush(stdout);
            free_argv(argv, argc);
            _exit(1);
        }
        // Parent: set process group for all children
        if (pids[i] > 0) {
            if (i == 0) {
                pgid = pids[0];
                setpgid(pids[i], 0);  // First child creates new process group
            } else {
                setpgid(pids[i], pgid);  // Other children join the group
            }
        }
        // Parent continues to next stage
    }

    // Set fg_pgid for signal handlers
    fg_pgid = pgid;
    // Give terminal to child group (only if we have a controlling terminal)
    if (pgid > 0 && isatty(STDIN_FILENO) && tcgetpgrp(STDIN_FILENO) != -1) {
        tcsetpgrp(STDIN_FILENO, pgid);
    }
    // Parent: close all pipes and wait for children
    if (pipes) {
        for (int i = 0; i < stages - 1; ++i) { close(pipes[i][0]); close(pipes[i][1]); }
        free(pipes);
    }

    int stopped = 0;
    int stopped_idx = -1;
    int status;
    for (int i = 0; i < stages; ++i) {
        if (pids[i] > 0) {
            pid_t w = waitpid(pids[i], &status, WUNTRACED);
            if (w > 0 && WIFSTOPPED(status)) {
                stopped = 1;
                stopped_idx = i;
            }
        }
    }
    // Restore terminal to shell (only if we have a controlling terminal)
    if (isatty(STDIN_FILENO) && tcgetpgrp(STDIN_FILENO) != -1) {
        tcsetpgrp(STDIN_FILENO, getpgrp());
    }
    fg_pgid = -1;
    // If stopped, add to bgjobs and print
    if (stopped && stopped_idx >= 0) {
        int jobnum = bgjobs_add(pids[stopped_idx], atoms[stopped_idx]);
        printf("[%d] Stopped %s\n", jobnum, atoms[stopped_idx]);
        fflush(stdout);
    }
    free(pids);
    for (int i = 0; i < stages; ++i) free(atoms[i]);
    free(atoms);
    return 1;

fail_split:
    if (atoms) {
        for (int i = 0; i < stages; ++i) free(atoms[i]);
        free(atoms);
    }
    return 0;
}

// Execute standard (non-builtin) commands from parsed structure
int execute_standard_commands(ShellTop *top) 
{
    int commands_executed = 0;
    
    for (int i = 0; i < top->count; i++) {
        const char *group = top->group[i];
        // Skip if it's a hop or log command (but not reveal - reveal needs execution for redirection)
        if (is_hop_atomic(group) || is_log_atomic(group)) {
            continue;
        }
        int run_in_bg = (i < top->count - 1 && top->sep[i] == '&');
        if (run_in_bg) {
            // Background job: fork, do not wait, print job info, redirect stdin, set process group
            pid_t pid = fork();
            if (pid == 0) {
                setpgid(0, 0);
                int devnull = open("/dev/null", O_RDONLY);
                if (devnull >= 0) {
                    dup2(devnull, STDIN_FILENO);
                    close(devnull);
                }
                if (strchr(group, '|') != NULL) {
                    execute_pipeline_group(group);
                } else {
                    execute_atomic_command(group);
                }
                _exit(0);
            } else if (pid > 0) {
                setpgid(pid, pid);
                int jobnum = bgjobs_add(pid, group);
                printf("[%d] %d\n", jobnum, pid);
                fflush(stdout);
            } else {
                perror("fork failed");
            }
        } else {
            // Foreground: wait for completion
            if (strchr(group, '|') != NULL) {
                if (execute_pipeline_group(group)) {
                    commands_executed++;
                } else {
                    printf("Failed to execute pipeline: %s\n", group);
                }
            } else {
                if (execute_atomic_command(group)) {
                    commands_executed++;
                } else {
                    printf("Failed to execute command: %s\n", group);
                }
            }
        }
        // Continue to next command in sequence
    }
    
    return commands_executed;
}

//######## LLM Generated Code ends ##############