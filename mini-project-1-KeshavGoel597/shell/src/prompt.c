#include "definition.h"
char *prevdir = NULL; // Only definition in shell sources

int main()
{
      // ############## LLM Generated Code Begins ##############
    log_init();
    bgjobs_init();
    install_shell_signal_handlers();
    // ############## LLM Generated Code ends ##############
    prevdir = malloc(size);
    if (prevdir) prevdir[0] = '\0'; // Ensure prevdir is always empty at startup
    char *home = malloc(size);
    getcwd(home, size);
    int len = strlen(home);

    while (1)
    {
          // ############## LLM Generated Code Begins ##############
        // Check for completed background jobs before showing prompt
        bgjobs_check_and_report();
        // Print prompt only after all commands in the sequence have finished
        //############## LLM Generated Code ends ############## 
        char *user = getenv("USER");
        if (user)
        {
            printf("<%s@", user);
        }
        else
        {
            perror("getenv");
        }
        // ############## LLM Generated Code Begins ##############
        struct utsname sys_info;
        if (uname(&sys_info) == 0)
        {
            printf("%s:", sys_info.nodename);
        }
        else
        {
            perror("uname");
        }
        // ############## LLM Generated Code ends ##############
        char *directory = malloc(size);
        if (getcwd(directory, size))
        {
            if (strncmp(directory, home, len) == 0)
            {
                // printf("test flag removed\n");
                directory += len; // Skip the home directory part
                printf("~%s", directory);
            }
            else
            {
                // printf("test flag not removed\n");
                printf("%s", directory);
            }
        }
        else
        {
            perror("getcwd");
        }

        // free(home);
        // free(directory);

        printf("> ");
        fflush(stdout); // Ensure the prompt is printed immediately


        char *input = malloc(MAX_INPUT_SIZE);
        if (fgets(input, MAX_INPUT_SIZE, stdin) == NULL) {
            // Ctrl-D (EOF) detected
            // Kill all child jobs
            for (int i = 0; i < MAX_BGJOBS; ++i) {
                if (bgjobs[i].active) {
                    kill(bgjobs[i].pid, SIGKILL);
                }
            }
            printf("logout\n");
            fflush(stdout);
            log_shutdown();
            exit(0);
        }
        input[strcspn(input, "\n")] = '\0';

          // Special: activities command
        if (strcmp(input, "activities") == 0) {
            bgjobs_activities();
            free(input);
            // free(directory);
            continue;
        }

//###### LLM Generated Code Begins ##############
        // Special: fg command
        if (strncmp(input, "fg", 2) == 0 && (input[2] == ' ' || input[2] == '\0')) {
            int jobnum = 0;
            if (input[2] == ' ') {
                char *rest = input + 2;
                while (*rest && isspace((unsigned char)*rest)) rest++;
                if (*rest) jobnum = atoi(rest);
            }
            fg_job(jobnum);
            free(input);
            free(directory);
            continue;
        }

        // Special: bg command
        if (strncmp(input, "bg", 2) == 0 && (input[2] == ' ' || input[2] == '\0')) {
            int jobnum = 0;
            if (input[2] == ' ') {
                char *rest = input + 2;
                while (*rest && isspace((unsigned char)*rest)) rest++;
                if (*rest) jobnum = atoi(rest);
            }
            bg_job(jobnum);
            free(input);
            free(directory);
            continue;
        }



           // Special: ping command
        if (strncmp(input, "ping ", 5) == 0) {
            char *rest = input + 5;
            while (*rest && isspace((unsigned char)*rest)) rest++;
            char *pid_str = rest;
            char *sig_str = NULL;
            while (*rest && !isspace((unsigned char)*rest)) rest++;
            if (*rest) { *rest = '\0'; rest++; }
            while (*rest && isspace((unsigned char)*rest)) rest++;
            sig_str = rest;
            if (pid_str && *pid_str && sig_str && *sig_str) {
                pid_t pid = (pid_t)atoi(pid_str);
                int sig = atoi(sig_str);
                pingfunc(pid, sig);
            } else {
                printf("Invalid Syntax!\n");
            }
            free(input);
            // free(directory);
            continue;
        }

        

        // Parse and execute the full shell_cmd (may contain multiple groups separated by ';' or '&')
        if (!is_valid_shell_cmd(input))
        {
            printf("Invalid Syntax!\n");
            free(input);
            // free(directory);
            continue;
        }

        ShellTop top = {0};
        if (!split_shell_cmd(input, &top))
        {
            // Still log the command if not a log command
            log_add_if_allowed(input, &top);
            printf("Invalid Syntax!\n");
            free(input);
            // free(directory);
            continue;
        }

        // Log the command (even if it will fail validation), unless it contains log atomic
        log_add_if_allowed(input, &top);

        // Validate each cmd_group against cmd_group -> atomic (| atomic)*
        int all_ok = 1;
        for (int i = 0; i < top.count; ++i)
        {
            if (!validate_cmd_group(top.group[i]))
            {
                all_ok = 0;
                break;
            }
        }
        if (!all_ok)
        {
            printf("Invalid Syntax!\n");
            free_shell_top(&top);
            free(input);
            // free(directory);
            continue;
        }

        // Handle log builtin first
        char exec_buf[MAX_INPUT_SIZE] = {0};
        int log_res = handle_log_command(&top, exec_buf, sizeof(exec_buf));
        if (log_res == 1) {
            // handled (print/purge); do not store
            free_shell_top(&top);
            free(input);
            // free(directory);
            continue;
        } else if (log_res == 2) {
            // execute stored command string without storing it again
            // Re-validate and process exec_buf using a fresh ShellTop
            ShellTop exec_top = (ShellTop){0};
            if (!is_valid_shell_cmd(exec_buf) || !split_shell_cmd(exec_buf, &exec_top)) {
                printf("Invalid Syntax!\n");
                free_shell_top(&top);
                free(input);
                // free(directory);
                continue;
            }
            int ok2 = 1;
            for (int i = 0; i < exec_top.count; ++i) if (!validate_cmd_group(exec_top.group[i])) { ok2 = 0; break; }
            if (!ok2) {
                printf("Invalid Syntax!\n");
                free_shell_top(&top);
                free_shell_top(&exec_top);
                free(input);
                // free(directory);
                continue;
            }
            // Replace top with exec_top for execution and fallthrough without logging
            free_shell_top(&top);
            top = exec_top; // take ownership of exec_top buffers
        } else {
            // Not a log command: add to history if allowed
            log_add_if_allowed(input, &top);
        }

        // Sequential execution: process all groups in order, waiting for each
        if (process_hop_commands(&top, home)) {
            // Hop command processed, skip other execution
        }
        // Let reveal and other builtins be handled by execute_standard_commands
        // which has proper redirection support
        execute_standard_commands(&top);




        // For now do nothing if valid; free and re-prompt
        free_shell_top(&top);

    // Valid at top level: for now, do nothing and re-prompt.
        // Later you'll recursively validate cmd_group pieces here.
        free(input);
        // free(directory);

        // if (strcmp(input, "exit\n") == 0 || strcmp(input, "quit\n") == 0)
        // {
        //     // free(input);
        //     break;
        // }
    }
    log_shutdown();
    //############## LLM Generated Code ends ##############
    return 0;
}