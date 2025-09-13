#include "definition.h"
// ############## LLM Generated Code Begins ##############
// #include <regex.h>   // add this
// #include <ctype.h>   // optional: for isspace in blank check
// ...existing code...

// Validate top-level: shell_cmd -> cmd_group ((&|;) cmd_group)* &?
// Arbitrary whitespace allowed around separators.


// Validate top-level: shell_cmd -> cmd_group ((&|;) cmd_group)* &?
// Arbitrary whitespace allowed around separators.
char *trim_dup(const char *b, const char *e) 
{
    // returns malloc'd trimmed substring [b,e)
    while (b < e && isspace((unsigned char)*b)) b++;
    while (e > b && isspace((unsigned char)e[-1])) e--;
    size_t n = (size_t)(e - b);
    char *out = malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, b, n);
    out[n] = '\0';
    return out;
}

// static int reg_full_match(const char *pattern, const char *s) {
//     regex_t re;
//     int rc = regcomp(&re, pattern, REG_EXTENDED);
//     if (rc != 0) return 0;
//     rc = regexec(&re, s, 0, NULL, 0);
//     regfree(&re);
//     return rc == 0;
// }

static int reg_full_match(const char *pattern, const char *s) 
{
    regex_t re;
    if (regcomp(&re, pattern, REG_EXTENDED)) return 0;
    int ok = (regexec(&re, s, 0, NULL, 0) == 0);
    regfree(&re);
    return ok;
}

// static char *trim_dup(const char *b, const char *e) {
//     while (b < e && isspace((unsigned char)*b)) b++;
//     while (e > b && isspace((unsigned char)e[-1])) e--;
//     size_t n = (size_t)(e - b);
//     char *out = (char *)malloc(n + 1);
//     if (!out) return NULL;
//     memcpy(out, b, n);
//     out[n] = '\0';
//     return out;
// }

// Validate top-level: shell_cmd -> cmd_group ((&|;) cmd_group)* &?
int is_valid_shell_cmd(const char *s) 
{
    // allow blank/whitespace-only as valid (shell does nothing)
    const unsigned char *p = (const unsigned char *)s;
    while (*p && isspace(*p)) p++;
    if (*p == '\0') return 1;

    // Ensure no empty groups around separators and optional trailing &
    // ^\s*([^&;]+)(\s*([&;]\s*[^&;]+))*\s*&?\s*$
    const char *pattern =
        "^[[:space:]]*"
        "([^&;]+)"
        "([[:space:]]*[&;][[:space:]]*[^&;]+)*"
        "[[:space:]]*&?[[:space:]]*$";
    return reg_full_match(pattern, s);
}

// Split validated shell_cmd into cmd_groups and separators; record trailing '&'
int split_shell_cmd(const char *s, ShellTop *out) 
{
    memset(out, 0, sizeof(*out));

    const char *beg = s;
    while (*beg && isspace((unsigned char)*beg)) beg++;

    const char *end = s + strlen(s);
    while (end > beg && isspace((unsigned char)end[-1])) end--;

    // detect trailing '&'
    if (end > beg && end[-1] == '&') {
        out->trailing_amp = 1;
        end--;
        while (end > beg && isspace((unsigned char)end[-1])) end--;
    }

    const char *seg_start = beg;
    for (const char *p = beg; p < end; ++p) {
        if (*p == ';' || *p == '&') {
            char *g = trim_dup(seg_start, p);
            if (!g || g[0] == '\0') { free(g); free_shell_top(out); return 0; }
            if (out->count >= MAX_GROUPS) { free(g); free_shell_top(out); return 0; }
            out->group[out->count] = g;
            out->sep[out->count] = *p;
            out->count++;
            seg_start = p + 1;
        }
    }
    // last segment
    char *g = trim_dup(seg_start, end);
    if (!g || g[0] == '\0') { free(g); free_shell_top(out); return 0; }
    if (out->count >= MAX_GROUPS) { free(g); free_shell_top(out); return 0; }
    out->group[out->count] = g;
    out->sep[out->count] = 0;
    out->count++;

    return 1;
}

void free_shell_top(ShellTop *st) 
{
    for (int i = 0; i < st->count; ++i) {
        free(st->group[i]);
        st->group[i] = NULL;
        st->sep[i] = 0;
    }
    st->count = 0;
    st->trailing_amp = 0;
}

// Validate a single atomic:
// atomic -> NAME1 WS+ ( NAME2 | input | output )*
// name   -> [^|&><;]+
// input  -> < WS? NAME
// output -> (>|>>) WS? NAME
// static int validate_atomic(const char *s) {
//     // ^\s*([^|&><;]+)\s+(([^|&><;]+)|(<\s*[^|&><;]+)|((>>|>)\s*[^|&><;]+))*\s*$
//     const char *pat =
//         "^[[:space:]]*"
//         "([^|&><;]+)"
//         "[[:space:]]+"
//         "("
//           "([^|&><;]+)"
//           "|(<[[:space:]]*[^|&><;]+)"
//           "|((>>|>)[[:space:]]*[^|&><;]+)"
//         ")*"
//         "[[:space:]]*$";
//     return reg_full_match(pat, s);
// }

// ...existing code...
// atomic -> NAME ( NAME | input | output )*
// name   -> [^|&><;]+
// input  -> < WS? NAME
// output -> (>|>>) WS? NAME
static int validate_atomic(const char *s) 
{
    // Accept:
    // - optional leading/trailing WS
    // - first NAME
    // - then zero or more of:
    //   - WS+ NAME
    //   - WS* '<' WS* NAME
    //   - WS* '>' or '>>' WS* NAME
    const char *pat =
        "^[[:space:]]*"              /* leading WS */
        "([^|&><;]+)"                /* NAME1 */
        "("
          "[[:space:]]+[^|&><;]+"    /* WS+ NAME2 */
          "|[[:space:]]*<[[:space:]]*[^|&><;]+"      /* input */
          "|[[:space:]]*(>>|>)[[:space:]]*[^|&><;]+" /* output */
        ")*"
        "[[:space:]]*$";             /* trailing WS */
    return reg_full_match(pat, s);
}
// ...existing code...

// Validate cmd_group: split by '|' and each part must be a valid atomic
int validate_cmd_group(const char *group) 
{
    const char *beg = group;
    while (*beg && isspace((unsigned char)*beg)) beg++;
    const char *end = group + strlen(group);
    while (end > beg && isspace((unsigned char)end[-1])) end--;

    if (beg >= end) return 0; // empty group not allowed

    const char *seg_start = beg;
    for (const char *p = beg; p < end; ++p) {
        if (*p == '|') {
            char *atom = trim_dup(seg_start, p);
            if (!atom || atom[0] == '\0') { free(atom); return 0; }
            int ok = validate_atomic(atom);
            free(atom);
            if (!ok) return 0;
            seg_start = p + 1;
        }
    }
    // last atomic
    char *atom = trim_dup(seg_start, end);
    if (!atom || atom[0] == '\0') { free(atom); return 0; }
    int ok = validate_atomic(atom);
    free(atom);
    return ok;
}
// ...existing code...






// Add this simple function to your regex.c file

// Check if the first word of an atomic command is "hop"
int is_hop_atomic(const char *atomic_cmd) 
{
    const char *p = atomic_cmd;
    
    // Skip leading whitespace
    while (*p && isspace((unsigned char)*p)) p++;
    
    // Check if it starts with "hop"
    if (strncmp(p, "hop", 3) != 0) return 0;
    
    // Make sure "hop" is a complete word (followed by whitespace, end, or redirect)
    p += 3;
    if (*p != '\0' && !isspace((unsigned char)*p)) return 0;
    
    return 1;
}

// Check if the first word of an atomic command is "log"
int is_log_atomic(const char *atomic_cmd)
{
    const char *p = atomic_cmd;
    while (*p && isspace((unsigned char)*p)) p++;
    if (strncmp(p, "log", 3) != 0) return 0;
    p += 3;
    if (*p != '\0' && !isspace((unsigned char)*p)) return 0;
    return 1;
}

// // Extract argument after "hop" (if any)
// static char* get_hop_argument(const char *atomic_cmd) {
//     const char *p = atomic_cmd;
    
//     // Skip to after "hop"
//     while (*p && isspace((unsigned char)*p)) p++;  // skip leading whitespace
//     p += 3;  // skip "hop"
//     while (*p && isspace((unsigned char)*p)) p++;  // skip whitespace after hop
    
//     if (*p == '\0') {
//         return NULL;  // No argument
//     }
    
//     // Find end of argument (next whitespace or end of string)
//     const char *arg_start = p;
//     while (*p && !isspace((unsigned char)*p) && *p != '<' && *p != '>' && *p != '|') p++;
    
//     // Create argument string
//     int len = p - arg_start;
//     char *arg = malloc(len + 1);
//     if (!arg) return NULL;
    
//     memcpy(arg, arg_start, len);
//     arg[len] = '\0';
//     return arg;
// }

// // Process hop commands in the parsed structure
// int process_hop_commands(ShellTop *top, char *home) {
//     int hop_found = 0;
    
//     for (int i = 0; i < top->count; i++) {
//         const char *group = top->group[i];
        
//         // Check if first atomic in this group is hop
//         // (hop commands shouldn't be in pipelines anyway)
//         if (is_hop_atomic(group)) {
//             char *arg = get_hop_argument(group);
            
//             if (arg == NULL) {
//                 // hop with no argument
//                 hopfunc(NULL, home);
//             } else {
//                 // hop with argument
//                 hopfunc(arg, home);
//                 free(arg);
//             }
            
//             hop_found = 1;
//             // You can break here if you want to process only first hop
//         }
//     }
    
//     return hop_found;
// }


// Extract all arguments after "hop" and process them sequentially
static void process_hop_arguments(const char *atomic_cmd, char *home) 
{
    const char *p = atomic_cmd;
    
    // Skip to after "hop"
    while (*p && isspace((unsigned char)*p)) p++;  // skip leading whitespace
    p += 3;  // skip "hop"
    while (*p && isspace((unsigned char)*p)) p++;  // skip whitespace after hop
    
    if (*p == '\0') {
        // No arguments - go to home
        hopfunc(NULL, home);
        return;
    }
    
    // Process all arguments sequentially
    while (*p != '\0' && *p != '<' && *p != '>' && *p != '|') {
        // Skip any whitespace
        while (*p && isspace((unsigned char)*p)) p++;
        
        if (*p == '\0' || *p == '<' || *p == '>' || *p == '|') break;
        
        // Find end of current argument
        const char *arg_start = p;
        while (*p && !isspace((unsigned char)*p) && *p != '<' && *p != '>' && *p != '|') p++;
        
        // Extract current argument
        int len = p - arg_start;
        char *arg = malloc(len + 1);
        if (!arg) break;  // Memory allocation failed
        
        memcpy(arg, arg_start, len);
        arg[len] = '\0';
        
        // Process this argument
        hopfunc(arg, home);
        
        free(arg);
    }
}

// Process hop commands in the parsed structure
int process_hop_commands(ShellTop *top, char *home) 
{
    int hop_found = 0;
    
    for (int i = 0; i < top->count; i++) {
        const char *group = top->group[i];
        
        // Check if first atomic in this group is hop
        // (hop commands shouldn't be in pipelines anyway)
        if (is_hop_atomic(group)) {
            // Check if there's redirection - if so, let standard execution handle it
            if (strchr(group, '>') || strchr(group, '<')) {
                continue; // Let execute_standard_commands handle this
            }
            
            process_hop_arguments(group, home);
            hop_found = 1;
            // You can break here if you want to process only first hop
        }
    }
    
    return hop_found;
}
// int is_valid_shell_cmd(const char *s) {
//     const unsigned char *p = (const unsigned char *)s;
//     while (*p && isspace(*p)) p++;
//     if (*p == '\0') return 1; // treat blank as valid (do nothing)

//     // POSIX ERE approximating the provided Python regex
//     const char *pattern =
//         "^[[:space:]]*"
//         "(.+?)" // cmd_group1 (non-greedy to allow following separators)
//         "[[:space:]]*"
//         "([&;][[:space:]]*(.+))+[[:space:]]*|"
//         "^[[:space:]]*(.+)[[:space:]]*&?[[:space:]]*$";

//     // The above is two alternatives:
//     // 1) cg ([&;] cg)+  and optional ws
//     // 2) single cg with optional trailing &

//     regex_t re;
//     if (regcomp(&re, pattern, REG_EXTENDED)) return 0;
//     int ok = (regexec(&re, s, 0, NULL, 0) == 0);
//     regfree(&re);
//     return ok;
// }


// Split validated shell_cmd into cmd_groups and separators; record trailing '&'
// int split_shell_cmd(const char *s, ShellTop *out) {
//     memset(out, 0, sizeof(*out));

//     const char *beg = s;
//     while (*beg && isspace((unsigned char)*beg)) beg++;

//     // Find effective end, detect trailing '&'
//     const char *end = s + strlen(s);
//     while (end > beg && isspace((unsigned char)end[-1])) end--;
//     if (end > beg && end[-1] == '&') {
//         out->trailing_amp = 1;
//         end--;
//         while (end > beg && isspace((unsigned char)end[-1])) end--;
//     }

//     const char *seg_start = beg;
//     for (const char *p = beg; p < end; ++p) {
//         if (*p == ';' || *p == '&') {
//             // push [seg_start, p)
//             char *g = trim_dup(seg_start, p);
//             if (!g || g[0] == '\0') { free(g); free_shell_top(out); return 0; }
//             if (out->count >= MAX_GROUPS) { free(g); free_shell_top(out); return 0; }
//             out->group[out->count] = g;
//             out->sep[out->count] = *p; // separator after this group
//             out->count++;
//             // next segment
//             seg_start = p + 1;
//         }
//     }
//     // push last segment [seg_start, end)
//     char *g = trim_dup(seg_start, end);
//     if (!g || g[0] == '\0') { free(g); free_shell_top(out); return 0; }
//     if (out->count >= MAX_GROUPS) { free(g); free_shell_top(out); return 0; }
//     out->group[out->count] = g;
//     out->sep[out->count] = 0; // no separator after last
//     out->count++;

//     return 1;
// }

// void free_shell_top(ShellTop *st) {
//     for (int i = 0; i < st->count; ++i) {
//         free(st->group[i]);
//         st->group[i] = NULL;
//         st->sep[i] = 0;
//     }
//     st->count = 0;
//     st->trailing_amp = 0;
// }


// ############## LLM Generated Code ends ##############





