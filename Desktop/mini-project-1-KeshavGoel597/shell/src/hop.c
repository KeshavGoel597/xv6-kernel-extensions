#include "definition.h"
void hopfunc(char *input, char *home)
{
    if (input == NULL || strcmp(input, "~") == 0)
    {
        // printf("test-000\n");
        if (getcwd(prevdir, size) == NULL) prevdir[0] = '\0';
        chdir(home);
    }
    else if (strcmp(input, ".") == 0)
    {
        // printf("test-1\n");
        // Do nothing, stay in the current directory
    }
    else if (strcmp(input, "..") == 0)
    {
        // printf("test-2\n");
        if (getcwd(prevdir, size) == NULL) prevdir[0] = '\0';
        chdir("..");
    }
    else if (strcmp(input, "-") == 0)
    {
        // printf("test-3\n");
        if(!prevdir || prevdir[0]=='\0')
        {
           //do nothing
           return;
        }
        else
        {
            char *current_dir = malloc(size);
            if (getcwd(current_dir, size) == NULL) current_dir[0] = '\0';
            chdir(prevdir);  //changed directory to the previous directory 
            strcpy(prevdir, current_dir);
            // free(current_dir);
        }
    }
    else
    {
        // printf("INPUT:%sENDED\n", input);
        // printf("test-4\n");
         // ############## LLM Generated Code Begins ##############
        struct stat st;
        if (stat(input, &st) == 0 && S_ISDIR(st.st_mode))
        {
     // ############## LLM Generated Code ends ##############
            if (getcwd(prevdir, size) == NULL) prevdir[0] = '\0';
            chdir(input);
        }
        else
        {
            printf("No such directory!\n");
        }
    }
    return;
}