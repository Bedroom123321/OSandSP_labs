#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

void env_from_file(const char *filename)
{
    FILE *file = fopen(filename, "r");
    if (!file)
    {
        perror("Error opening env");
        exit(EXIT_FAILURE);
    }

    char buffer[256];
    while (fgets(buffer, sizeof(buffer), file))
    {
        buffer[strcspn(buffer, "\n")] = 0;
        char *value = getenv(buffer);
        if (value)
        {
            printf("%s=%s\n", buffer, value);
        }
    }
    fclose(file);
}

void env_from_envp(char **envp)
{
    for (char** env = envp; *env; env++)
    {
        printf("%s\n", *env);
    }
}

int main(int argc, char *argv[], char *envp[])
{
    printf("Process: %s, PID: %d, PPID: %d\n", argv[0], getpid(), getppid());

    if (argc > 1)
    {
        env_from_file(argv[1]);
    }
    else
    {
        env_from_envp(envp);
    }

    return 0;
}