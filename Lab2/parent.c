#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <locale.h>

#define MAX_CHILD 99
#define MAX_ENV 1024

int compare(const void *a, const void *b) {
    return strcoll(*(const char **)a, *(const char **)b);
}

void print_env_vars(void) {
    extern char **environ;
    char *env_list[MAX_ENV];
    int count = 0;

    for (char **env = environ; *env != NULL; env++) {
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

    char buffer[256];
    int count = 0;
    while (fgets(buffer, sizeof(buffer), file))
    {
        buffer[strcspn(buffer, "\n")] = 0;
        char *value = getenv(buffer);
        if (value)
        {
            size_t length = strlen(buffer) + strlen(value) + 2;
            envp[count] = malloc(length);
            if (!envp[count])
            {
                perror("Error malloc");
                exit(EXIT_FAILURE);
            }
            snprintf(envp[count], length, "%s=%s", buffer, value);
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

    char child[256];
    snprintf(child, sizeof(child), "%s/child", child_path);

    char child_name[16];
    snprintf(child_name, sizeof(child_name), "child_%02d", child_num);

    char *envp[10];
    int env_count;
    read_env_file(envp, &env_count);

    if (mode == 1)
    {
        char *argv[] = {child_name, "env.txt", NULL};
        if (fork() == 0)
        {
            execve(child, argv, envp);
        }
    }
    else if (mode == 0)
    {
        char *argv[] = {child_name, NULL};
        if (fork() == 0)
        {
            execve(child, argv, envp);
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
    char symbol;
    while (1)
    {
        symbol = getchar();
        if (symbol == '+')
        {
            if (child_num < MAX_CHILD) {
                creat_child(child_num++, 1);
            }
        }
        else if (symbol == '*')
        {
            if (child_num < MAX_CHILD) {
                creat_child(child_num++, 0);
            }
        }
        else if (symbol == 'q')
        {
            break;
        }
    }

    while (wait(NULL) > 0);
    return 0;
}