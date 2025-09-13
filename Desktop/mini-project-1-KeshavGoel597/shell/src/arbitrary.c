//ISSUE: COMMANDS LIKE EXIT ARE FAILING

#include "definition.h"

// ############## LLM Generated Code Begins ##############
// Add these functions to handle standard command execution

// Builtin: cd
int builtin_cd(char **argv, int argc)
{
    // prevdir is a global (extern) from definition.h
    // Behavior:
    //   cd            -> HOME
    //   cd ~          -> HOME
    //   cd -          -> switch to prevdir
    //   cd <path>     -> chdir(<path>)
    // Updates prevdir to old cwd on success.

    // Resolve target
    const char *target = NULL;
    if (argc < 2 || argv[1] == NULL || strcmp(argv[1], "~") == 0) {
        target = getenv("HOME");
        if (!target) target = "/"; // fallback
    } else if (strcmp(argv[1], "-") == 0) {
        if (!prevdir || prevdir[0] == '\0') {
            fprintf(stderr, "No such directory!\n");
            return 0;
        }
        target = prevdir;
    } else if (argv[1][0] == '~' && argv[1][1] == '/' ) {
        // simple ~/<rest> expansion
        const char *home = getenv("HOME");
        if (!home) home = "/";
        size_t hlen = strlen(home);
        size_t rest = strlen(argv[1] + 1); // includes '/...'
        char *buf = (char *)malloc(hlen + rest + 1);
        if (!buf) { perror("malloc"); return 0; }
        memcpy(buf, home, hlen);
        memcpy(buf + hlen, argv[1] + 1, rest + 1);
        // Perform chdir with constructed path, then free
        char cwd[size];
        if (!getcwd(cwd, sizeof(cwd))) cwd[0] = '\0';
        if (chdir(buf) != 0) { perror("cd"); free(buf); return 0; }
        if (!prevdir) prevdir = (char *)malloc(size);
        if (prevdir) { strncpy(prevdir, cwd, size - 1); prevdir[size - 1] = '\0'; }
        free(buf);
        return 1;
    } else {
        target = argv[1];
    }

    char cwd[size];
    if (!getcwd(cwd, sizeof(cwd))) cwd[0] = '\0';
    if (chdir(target) != 0) {
        perror("cd");
        return 0;
    }
    if (!prevdir) prevdir = (char *)malloc(size);
    if (prevdir) { strncpy(prevdir, cwd, size - 1); prevdir[size - 1] = '\0'; }
    return 1;
}

// Parse an atomic command into command and arguments
int parse_atomic_to_argv(const char *atomic_cmd, char ***argv, int *argc) 
{
    *argv = NULL;
    *argc = 0;
    
    const char *p = atomic_cmd;
    char *temp_args[64]; // temporary storage
    int temp_argc = 0;
    
    // Skip leading whitespace
    while (*p && isspace((unsigned char)*p)) p++;
    
    // Parse command and arguments
    while (*p && temp_argc < 64) {
        if (*p == '<' || *p == '>' || *p == '|') {
            // Stop at redirections or pipes
            break;
        }
        
        // Skip whitespace
        while (*p && isspace((unsigned char)*p)) p++;
        if (*p == '\0' || *p == '<' || *p == '>' || *p == '|') break;
        
        const char *arg_start = p;
        // Find end of argument (next whitespace, redirection, or pipe)
        while (*p && !isspace((unsigned char)*p) && *p != '<' && *p != '>' && *p != '|') p++;
        
        // Extract argument
        int arg_len = p - arg_start;
        if (arg_len > 0) {
            temp_args[temp_argc] = malloc(arg_len + 1);
            if (!temp_args[temp_argc]) {
                // Free previously allocated args on error
                for (int i = 0; i < temp_argc; i++) {
                    free(temp_args[i]);
                }
                return 0;
            }
            
            memcpy(temp_args[temp_argc], arg_start, arg_len);
            temp_args[temp_argc][arg_len] = '\0';
            temp_argc++;
        }
    }
    
    if (temp_argc == 0) return 0; // No command found
    
    // Allocate final argv array (including NULL terminator)
    *argv = malloc((temp_argc + 1) * sizeof(char*));
    if (!*argv) {
        // Free temp args on error
        for (int i = 0; i < temp_argc; i++) {
            free(temp_args[i]);
        }
        return 0;
    }
    
    // Copy pointers and add NULL terminator
    for (int i = 0; i < temp_argc; i++) {
        (*argv)[i] = temp_args[i];
    }
    (*argv)[temp_argc] = NULL;
    
    *argc = temp_argc;
    return 1;
}

// Free argv array
void free_argv(char **argv, int argc) 
{
    if (argv) {
        for (int i = 0; i < argc; i++) {
            free(argv[i]);
        }
        free(argv);
    }
}

// Execute a single atomic command
int execute_atomic_command(const char *atomic_cmd) 
{
    char **argv;
    int argc;
    char *infile = NULL;
    char *outfile = NULL;
    int out_append = 0; // 0 for '>', 1 for '>>'
    
    // Parse command into argv format
    if (!parse_atomic_to_argv(atomic_cmd, &argv, &argc)) {
        return 0; // Parsing failed
    }
    
    // Builtins that don't need redirection (cd)
    if (argc > 0 && strcmp(argv[0], "cd") == 0) {
        int ok = builtin_cd(argv, argc);
        free_argv(argv, argc);
        return ok;
    }

    // Parse redirections in one pass: validate ALL redirections, fail on first error
    char *all_infiles[10];  // Support up to 10 input redirects
    char *all_outfiles[10]; // Support up to 10 output redirects
    int all_out_append[10];
    int num_infiles = 0, num_outfiles = 0;
    
    {
        const char *p = atomic_cmd;
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
                if (n > 0 && num_infiles < 10) {
                    char *tmp = (char *)malloc(n + 1);
                    if (!tmp) { free_argv(argv, argc); return 0; }
                    memcpy(tmp, start, n);
                    tmp[n] = '\0';
                    all_infiles[num_infiles++] = tmp;
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
                if (n > 0 && num_outfiles < 10) {
                    char *tmp = (char *)malloc(n + 1);
                    if (!tmp) { 
                        for (int i = 0; i < num_infiles; i++) free(all_infiles[i]);
                        free_argv(argv, argc); 
                        return 0; 
                    }
                    memcpy(tmp, start, n);
                    tmp[n] = '\0';
                    all_outfiles[num_outfiles] = tmp;
                    all_out_append[num_outfiles] = append;
                    num_outfiles++;
                }
                continue;
            }
            p++;
        }
    }

    // Validate all input files (fail on first error)
    for (int i = 0; i < num_infiles; i++) {
        int test_fd = open(all_infiles[i], O_RDONLY);
        if (test_fd < 0) {
            printf("No such file or directory\n");
            for (int j = 0; j < num_infiles; j++) free(all_infiles[j]);
            for (int j = 0; j < num_outfiles; j++) free(all_outfiles[j]);
            free_argv(argv, argc);
            return 0;
        }
        close(test_fd);
    }
    
    // Validate all output files (fail on first error) 
    for (int i = 0; i < num_outfiles; i++) {
        int flags = O_WRONLY | O_CREAT | (all_out_append[i] ? O_APPEND : O_TRUNC);
        int test_fd = open(all_outfiles[i], flags, 0666);
        if (test_fd < 0) {
            printf("Unable to create file for writing\n");
            for (int j = 0; j < num_infiles; j++) free(all_infiles[j]);
            for (int j = 0; j < num_outfiles; j++) free(all_outfiles[j]);
            free_argv(argv, argc);
            return 0;
        }
        close(test_fd);
    }
    
    // Use last input and output file (existing logic)
    if (num_infiles > 0) {
        infile = strdup(all_infiles[num_infiles - 1]);
    }
    if (num_outfiles > 0) {
        outfile = strdup(all_outfiles[num_outfiles - 1]);
        out_append = all_out_append[num_outfiles - 1];
    }
    
    // Free temporary arrays
    for (int i = 0; i < num_infiles; i++) free(all_infiles[i]);
    for (int j = 0; j < num_outfiles; j++) free(all_outfiles[j]);

    int fd_in = -1;
    if (infile) {
        fd_in = open(infile, O_RDONLY);
        // File should exist since we validated it above
        if (fd_in < 0) {
            // This should not happen, but handle gracefully
            printf("No such file or directory\n");
            free(infile);
            free(outfile);
            free_argv(argv, argc);
            return 0;
        }
    }

    int fd_out = -1;
    if (outfile) {
        int flags = O_WRONLY | O_CREAT | (out_append ? O_APPEND : O_TRUNC);
        fd_out = open(outfile, flags, 0666);
        // File should be creatable since we validated it above
        if (fd_out < 0) {
            // This should not happen, but handle gracefully
            printf("Unable to create file for writing\n");
            if (fd_in >= 0) close(fd_in);
            free(infile);
            free(outfile);
            free_argv(argv, argc);
            return 0;
        }
    }

    // Handle reveal as a builtin (after redirection setup)
    if (argc > 0 && strcmp(argv[0], "reveal") == 0) {
        // Save original stdout if needed
        int original_stdout = -1;
        if (fd_out >= 0) {
            original_stdout = dup(STDOUT_FILENO);
            if (dup2(fd_out, STDOUT_FILENO) < 0) {
                perror("dup2");
                if (fd_in >= 0) close(fd_in);
                close(fd_out);
                free(infile);
                free(outfile);
                free_argv(argv, argc);
                return 0;
            }
        }
        
        // Parse reveal-specific arguments
        int show_hidden = 0, long_format = 0;
        char *path = NULL;
        
        // Parse flags and path
        for (int i = 1; i < argc; i++) {
            if (argv[i][0] == '-' && argv[i][1] != '\0') {
                // This is a flag (like -a or -l)
                for (int j = 1; argv[i][j]; j++) {
                    if (argv[i][j] == 'a') show_hidden = 1;
                    else if (argv[i][j] == 'l') long_format = 1;
                    else {
                        printf("reveal: Invalid Syntax!\n");
                        if (original_stdout >= 0) {
                            dup2(original_stdout, STDOUT_FILENO);
                            close(original_stdout);
                        }
                        if (fd_in >= 0) close(fd_in);
                        if (fd_out >= 0) close(fd_out);
                        free(infile);
                        free(outfile);
                        free_argv(argv, argc);
                        return 0;
                    }
                }
            } else {
                // This is a path argument (including "-")
                if (path) {
                    // Multiple paths - invalid
                    printf("reveal: Invalid Syntax!\n");
                    free(path);
                    if (original_stdout >= 0) {
                        dup2(original_stdout, STDOUT_FILENO);
                        close(original_stdout);
                    }
                    if (fd_in >= 0) close(fd_in);
                    if (fd_out >= 0) close(fd_out);
                    free(infile);
                    free(outfile);
                    free_argv(argv, argc);
                    return 0;
                }
                path = strdup(argv[i]);
            }
        }
        
        // Get home directory - this is a hack since we don't have access to home here
        char *home = getenv("HOME");
        if (!home) home = "/";
        
        revfunc(show_hidden, long_format, path, home);
        
        // Restore original stdout if it was redirected
        if (original_stdout >= 0) {
            dup2(original_stdout, STDOUT_FILENO);
            close(original_stdout);
        }
        
        // Clean up
        if (fd_in >= 0) close(fd_in);
        if (fd_out >= 0) close(fd_out);
        free(infile);
        free(outfile);
        free(path);
        free_argv(argv, argc);
        return 1;
    }

    // Fork and execute external command
    pid_t pid = fork();
    
    if (pid == 0) {
        // Child process - execute the command
        if (fd_in >= 0) {
            if (dup2(fd_in, STDIN_FILENO) < 0) {
                perror("dup2");
                _exit(1);
            }
            close(fd_in);
        }
        if (fd_out >= 0) {
            if (dup2(fd_out, STDOUT_FILENO) < 0) {
                perror("dup2");
                _exit(1);
            }
            close(fd_out);
        }
        execvp(argv[0], argv);
        
        // If execvp returns, it failed
        printf("Command not found!\n");
        fflush(stdout);
        exit(1);
        
    } else if (pid > 0) {
        // Parent process - wait for child to complete
        int status;
        waitpid(pid, &status, 0);
        if (fd_in >= 0) close(fd_in);
    if (fd_out >= 0) close(fd_out);
        
    free(infile);
    free(outfile);
        free_argv(argv, argc);
        return 1; // Success
        
    } else {
        // Fork failed
        perror("fork failed");
        if (fd_in >= 0) close(fd_in);
    if (fd_out >= 0) close(fd_out);
        free(infile);
    free(outfile);
        free_argv(argv, argc);
        return 0;
    }
}

// Helper: duplicate and trim substring [b,e)
char *dup_trim_range(const char *b, const char *e) {
    while (b < e && isspace((unsigned char)*b)) b++;
    while (e > b && isspace((unsigned char)e[-1])) e--;
    size_t n = (size_t)(e - b);
    char *s = (char *)malloc(n + 1);
    if (!s) return NULL;
    memcpy(s, b, n);
    s[n] = '\0';
    return s;
}



// ############## LLM Generated Code Ends ##############