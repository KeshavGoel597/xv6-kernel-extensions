#include "definition.h"
int is_reveal_atomic(const char *atomic_cmd)
{
    const char *p = atomic_cmd;
    while (*p && isspace((unsigned char)*p)) p++;
    if (strncmp(p, "reveal", 6) != 0) return 0;
    p += 6;
    if (*p != '\0' && !isspace((unsigned char)*p)) return 0;
    return 1;
}

void revfunc(int show_hidden, int long_format, char* path, char* home)
{
    // printf("ENTERED REVFUNC WITH PATH: %s\n", path ? path : "NULL");
    DIR* dir = NULL;
    struct dirent* entry;
    char actual_path[size];
    // If path is NULL or empty, use current working directory
    if (path == NULL || path[0] == '\0') {
        if (getcwd(actual_path, sizeof(actual_path)) == NULL) {
            perror("getcwd");
            return;
        }
        dir = opendir(actual_path);
    } else if (strcmp(path, "~") == 0) {
        dir = opendir(home);
    } else if (strcmp(path, "-") == 0) {
        if (!prevdir || prevdir[0] == '\0') {
            printf("No such directory!\n");
            // printf("CASE 1:Previous Directory is NULL or Empty\n");
            return;
        } else {
          // printf("CASE 2:Previous Directory: %s\n", prevdir);
            dir = opendir(prevdir);
        }
    } else {
        struct stat st;
        if (stat(path, &st) == 0 && S_ISDIR(st.st_mode))  
            dir = opendir(path);
        else {
            printf("No such directory!\n");
            return;
        }
    }
    if (dir == NULL)
    {
        perror("opendir");
        return;
    }

    char** arr = (char**)malloc(size * sizeof(char*));
    int count = 0;
    while ((entry = readdir(dir)) != NULL && count < size)
    {
        if (!show_hidden && entry->d_name[0] == '.')
        {
            continue; // Skip hidden files unless -a is specified
        }
        arr[count] = strdup(entry->d_name);
        count++;
    }
    closedir(dir);
    // Sort the array of filenames
    for (int i = 0; i < count - 1; i++)
    {
        for (int j = i + 1; j < count; j++)
        {
            if (strcmp(arr[i], arr[j]) > 0)
            {
                char* temp = arr[i];
                arr[i] = arr[j];
                arr[j] = temp;  
            }
        }
    }
    if(long_format){
        for (int i = 0; i < count; i++)
        {
            printf("%s\n", arr[i]);
            free(arr[i]);
        }
    }
    else{
        for (int i = 0; i < count; i++)
        {
            printf("%s  ", arr[i]);
            free(arr[i]);
        }
        printf("\n");
    }
    free(arr);
    return;
}



// Parse reveal command arguments: -(a|l)* followed by optional path
// Returns: 1 if valid reveal command, 0 if not
int parse_reveal_args(const char *atomic_cmd, int *show_hidden, int *long_format, char **path) {
    const char *p = atomic_cmd;
    *show_hidden = 0;
    *long_format = 0;
    *path = NULL;
    
    // Skip to after "reveal"
    while (*p && isspace((unsigned char)*p)) p++;
    p += 6; // skip "reveal"
    while (*p && isspace((unsigned char)*p)) p++;
    
    // Parse flags: -(a|l)*
    while (*p == '-') {
        const char *flag_start = p;
        p++; // skip '-'
        if (*p == 'a') {
            *show_hidden = 1;
            p++;
        } else if (*p == 'l') {
            *long_format = 1;
            p++;
        } else {
            // Not a valid flag, treat as path (rewind to flag_start)
            p = flag_start;
            break;
        }
        // Skip whitespace after flags
        while (*p && isspace((unsigned char)*p)) p++;
    }
    
    // Parse optional path argument
    if (*p != '\0' && *p != '<' && *p != '>' && *p != '|') {
        const char *path_start = p;
        
        // Find end of path (next whitespace or redirect)
        while (*p && !isspace((unsigned char)*p) && *p != '<' && *p != '>' && *p != '|') p++;
        
        // Check if there are more arguments after the first path
        const char *check_p = p;
        while (*check_p && isspace((unsigned char)*check_p)) check_p++;
        if (*check_p != '\0' && *check_p != '<' && *check_p != '>' && *check_p != '|') {
            // Multiple path arguments found - this is invalid
            return 0; // Invalid syntax
        }
        
        // Extract path
        int len = p - path_start;
        *path = malloc(len + 1);
        if (!*path) return 0;
        
        memcpy(*path, path_start, len);
        (*path)[len] = '\0';
    }
    
    return 1;
}

// Process reveal commands in the parsed structure
int process_reveal_commands(ShellTop *top, char *home) {
    // No longer process reveal commands here - they all go through standard execution
    // This function is kept for compatibility but does nothing
    return 0;
}