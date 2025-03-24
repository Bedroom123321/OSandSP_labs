#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#define MAX_CHILD 99
#define MAX_ENV 1024

extern int setenv(const char *name, const char *value, int overwrite);

int compare(const void *a, const void *b) {
    return strcmp(*(const char **)a, *(const char **)b);
}

void print_env_vars(void) {
    extern char **environ;
    char *env_list[MAX_ENV];
    int count = 0;

    for (char **env = environ; *env; env++) {
        env_list[count++] = *env;
    }
    qsort(env_list, count, sizeof(char *), compare);

    for (int i = 0; i < count; i++) {
        printf("%s\n", env_list[i]);
    }
}

void read_env_file(char *envp[], int *env_count)
{
    FILE *file = fopen("env.txt", "r");
    if (!file)
    {
        perror("Error of opening env");
        exit(EXIT_FAILURE);
    }

    char line[256];
    int count = 0;
    while (fgets(line, sizeof(line), file))
    {
        line[strcspn(line, "\n")] = 0;
        char *value = getenv(line);
        if (value)
        {
            size_t length = strlen(line) + strlen(value) + 2;
            envp[count] = malloc(length);
            if (!envp[count])
            {
                perror("Error malloc");
                exit(EXIT_FAILURE);
            }
            snprintf(envp[count], length, "%s=%s", line, value);
            count++;
        }
    }
    envp[count] = NULL;
    *env_count = count;
    fclose(file);
}

void creat_child(int child_num, int mode)
{
    char *child_path = getenv("CHILD_PATH");
    if (!child_path)
    {
        fprintf(stderr, "Variable CHILD_PATH doesn't set\n");
        return;
    }

    char child_exec[256];
    snprintf(child_exec, sizeof(child_exec), "%s/child", child_path);

    char child_name[16];
    snprintf(child_name, sizeof(child_name), "child_%02d", child_num);

    if (access(child_exec, X_OK) != 0)
    {
        perror("Child acces error");
        return;
    }

    char *envp[11];
    int env_count;
    read_env_file(envp, &env_count);

    if (mode == 1)
    {
        char *argv[] = {child_name, "env.txt", NULL};
        if (fork() == 0)
        {
            execve(child_exec, argv, envp);
            perror("Error execve");
            exit(EXIT_FAILURE);
        }
    }
    else if (mode == 0)
    {
        char *argv[] = {child_name, NULL};
        if (fork() == 0)
        {
            execve(child_exec, argv, envp);
            perror("Error execve");
            exit(EXIT_FAILURE);
        }
    }

    for (int i = 0; i < env_count; i++)
    {
        free(envp[i]);
    }
}

int main(void)
{
    print_env_vars();

    int child_num = 0;
    char command;
    while (1)
    {
        command = getchar();
        if (command == '+')
        {
            if (child_num < MAX_CHILD) creat_child(child_num++, 1);
        }
        else if (command == '*')
        {
            if (child_num < MAX_CHILD) creat_child(child_num++, 0);
        }
        else if (command == 'q')
        {
            break;
        }
    }

    while (wait(NULL) > 0);
    return 0;
}