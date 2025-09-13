//ISSUE: Do not store any shell_cmd if the command name of an atomic command is log itself
// echo 'hello' | cat regex.c | log GETS STORED
//ISSUE: log;echo "hello" & touch temp.txt (LOG NOT WORKING)
#include "definition.h"

// Simple persistent ring buffer for command history
// File location: ~/.mini_shell_history

static char history[LOG_MAX][MAX_INPUT_SIZE];
static int count = 0;      // number of valid entries (<= LOG_MAX)
static int head = 0;       // index of the oldest entry

static const char *history_path(void) {
    static char path[PATH_MAX];
    const char *home = getenv("HOME");
    if (!home) home = ".";
    snprintf(path, sizeof(path), "%s/%s", home, ".mini_shell_history");
    return path;
}

static void history_load(void) {
    const char *path = history_path();
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[MAX_INPUT_SIZE];
    while (fgets(line, sizeof(line), f)) {
        size_t n = strlen(line);
        if (n && (line[n-1] == '\n' || line[n-1] == '\r')) line[--n] = '\0';
        // push into ring
        if (count < LOG_MAX) {
            strncpy(history[(head + count) % LOG_MAX], line, MAX_INPUT_SIZE-1);
            history[(head + count) % LOG_MAX][MAX_INPUT_SIZE-1] = '\0';
            count++;
        } else {
            // overwrite oldest
            strncpy(history[head], line, MAX_INPUT_SIZE-1);
            history[head][MAX_INPUT_SIZE-1] = '\0';
            head = (head + 1) % LOG_MAX;
        }
    }
    fclose(f);
}

static void history_save(void) {
    const char *path = history_path();
    FILE *f = fopen(path, "w");
    if (!f) return;
    // write oldest -> newest
    for (int i = 0; i < count; ++i) {
        int idx = (head + i) % LOG_MAX;
        fputs(history[idx], f);
        fputc('\n', f);
    }
    fclose(f);
}

void log_init(void) {
    history_load();
}

void log_shutdown(void) {
    history_save();
}

void log_print(void) {
    // Print oldest -> newest
    for (int i = 0; i < count; ++i) {
        int idx = (head + i) % LOG_MAX;
        printf("%s\n", history[idx]);
    }
}

void log_purge(void) {
    count = 0;
    head = 0;
    history_save();
}

// Do not store if identical to previous stored command
static int same_as_latest(const char *line) {
    if (count == 0) return 0;
    int latest = (head + count - 1) % LOG_MAX;
    return strcmp(history[latest], line) == 0;
}

// Returns 1 if any atomic in any group is 'log' (first token of any atomic)
int shell_contains_log(const ShellTop *top) {
    for (int i = 0; i < top->count; ++i) {
        const char *g = top->group[i];
        const char *p = g;
        while (*p) {
            // skip leading whitespace
            while (*p && isspace((unsigned char)*p)) p++;
            // find end of atomic (either '|' or end)
            const char *atomic_start = p;
            const char *atomic_end = p;
            while (*atomic_end && *atomic_end != '|') atomic_end++;
            // check first token of this atomic
            const char *tok = atomic_start;
            while (tok < atomic_end && isspace((unsigned char)*tok)) tok++;
            const char *tok_end = tok;
            while (tok_end < atomic_end && !isspace((unsigned char)*tok_end) && *tok_end!='<' && *tok_end!='>' && *tok_end!='|') tok_end++;
            size_t n = (size_t)(tok_end - tok);
            if (n == 3 && strncmp(tok, "log", 3) == 0) return 1;
            // move p to after this atomic
            p = atomic_end;
            if (*p == '|') p++;
        }
    }
    return 0;
}

void log_add_if_allowed(const char *line, const ShellTop *top) {
    if (!line || !*line) return;
    // Do not store if any atomic command name is "log"
    if (top && shell_contains_log(top)) return;
    // Do not store if identical to previous
    if (same_as_latest(line)) return;

    if (count < LOG_MAX) {
        strncpy(history[(head + count) % LOG_MAX], line, MAX_INPUT_SIZE-1);
        history[(head + count) % LOG_MAX][MAX_INPUT_SIZE-1] = '\0';
        count++;
    } else {
        strncpy(history[head], line, MAX_INPUT_SIZE-1);
        history[head][MAX_INPUT_SIZE-1] = '\0';
        head = (head + 1) % LOG_MAX;
    }
    history_save();
}

// Return 0 (not a log cmd), 1 (handled print/purge), or 2 (execute), and fill exec_buf for execute
int handle_log_command(const ShellTop *top, char *exec_buf, size_t buf_size) {
    if (!top || top->count != 1) return 0; // only treat as builtin if single group
    const char *g = top->group[0];
    const char *p = g;
    while (*p && isspace((unsigned char)*p)) p++;
    if (strncmp(p, "log", 3) != 0) return 0;
    p += 3;
    // skip ws
    while (*p && isspace((unsigned char)*p)) p++;
    if (*p == '\0') {
        log_print();
        return 1;
    }
    // read next token
    const char *arg_start = p;
    while (*p && !isspace((unsigned char)*p)) p++;
    size_t n = (size_t)(p - arg_start);
    if (n == 5 && strncmp(arg_start, "purge", 5) == 0) {
        log_purge();
        return 1;
    }
    if (n == 7 && strncmp(arg_start, "execute", 7) == 0) {
        // next token: index (1-indexed newest->oldest)
        while (*p && isspace((unsigned char)*p)) p++;
        if (!isdigit((unsigned char)*p)) {
            printf("invalid\n");
            return 1; // handled with error output
        }
        char *endptr = NULL;
        long k = strtol(p, &endptr, 10);
        if (k <= 0 || k > count) {
            printf("invalid\n");
            return 1;
        }
        // newest is (head + count - 1)
        int newest = (head + count - 1) % LOG_MAX;
        int idx = (newest - (int)(k - 1) + LOG_MAX) % LOG_MAX;
        if (exec_buf && buf_size > 0) {
            strncpy(exec_buf, history[idx], buf_size - 1);
            exec_buf[buf_size - 1] = '\0';
        }
        return 2;
    }
    printf("invalid\n");
    return 1;
}
