#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>

#define CYCLE_LIMIT 101
#define MAX_CHILDREN 100

/* Глобальные переменные */
pid_t children[MAX_CHILDREN];
int child_count = 0;

struct pair {
    int a;
    int b;
} data;

int count00 = 0, count01 = 0, count10 = 0, count11 = 0;
int cycle_count = 0;

void parent_sigusr1_handler(int signo, siginfo_t *info, void *context) {
    (void)signo; (void)context;
    pid_t child_pid = info->si_pid;
    printf("Parent (PID=%d): Received signal from child (PID=%d)\n",
           getpid(), child_pid);
    fflush(stdout);
    kill(child_pid, SIGUSR2);
}

void setup_parent_signal_handlers(void) {
    struct sigaction sa;
    sa.sa_sigaction = parent_sigusr1_handler;
    sa.sa_flags = SA_SIGINFO;
    sigemptyset(&sa.sa_mask);
    if(sigaction(SIGUSR1, &sa, NULL) == -1) {
        perror("parent sigaction");
        exit(EXIT_FAILURE);
    }
}

void child_sigusr1_handler(int signo) {
    (void)signo;
    int local_a = data.a;
    int local_b = data.b;

    if(local_a == 0 && local_b == 0) count00++;
    else if(local_a == 0 && local_b == 1) count01++;
    else if(local_a == 1 && local_b == 0) count10++;
    else if(local_a == 1 && local_b == 1) count11++;

    cycle_count++;
}

void child_sigusr2_handler(int signo) {
    (void)signo;
}

void setup_child_signal_handlers(void) {
    struct sigaction sa;
    sa.sa_handler = child_sigusr1_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if(sigaction(SIGUSR1, &sa, NULL) == -1) {
        perror("child sigaction SIGUSR1");
        exit(EXIT_FAILURE);
    }

    sa.sa_handler = child_sigusr2_handler;
    if(sigaction(SIGUSR2, &sa, NULL) == -1) {
        perror("child sigaction SIGUSR2");
        exit(EXIT_FAILURE);
    }
}

void child_process(void) {
    setup_child_signal_handlers();

    struct timespec ts = { .tv_sec = 0, .tv_nsec = 100 * 1000000 };
    int toggle = 0;

    for(;;) {
        nanosleep(&ts, NULL);

        if(toggle) {
            data.a = 0;
            for(int i = 0; i < 100000; i++); // Имитация работы
            data.b = 0;
        } else {
            data.a = 1;
            for(int i = 0; i < 100000; i++); // Имитация работы
            data.b = 1;
        }
        toggle = !toggle;

        raise(SIGUSR1);

        if(cycle_count >= CYCLE_LIMIT) {
            kill(getppid(), SIGUSR1);
            pause();
            printf("Child: PPID=%d, PID=%d, Stats: (0,0)=%d, (0,1)=%d, (1,0)=%d, (1,1)=%d\n",
                   getppid(), getpid(), count00, count01, count10, count11);
            fflush(stdout);

            /* Сброс статистики */
            count00 = count01 = count10 = count11 = 0;
            cycle_count = 0;
        }
    }
}

void create_child(void) {
    if(child_count >= MAX_CHILDREN) {
        printf("Maximum children limit (%d) reached\n", MAX_CHILDREN);
        return;
    }

    pid_t pid = fork();
    if(pid < 0) {
        perror("fork");
        return;
    }

    if(pid == 0) {
        child_process();
        exit(EXIT_SUCCESS);
    }

    children[child_count++] = pid;
    printf("Created child %d (PID=%d)\n", child_count, pid);
    fflush(stdout);
}

void kill_last_child(void) {
    if(child_count == 0) {
        printf("No children to kill\n");
        return;
    }

    pid_t pid = children[--child_count];
    kill(pid, SIGTERM);
    waitpid(pid, NULL, 0);
    printf("Killed child (PID=%d), remaining: %d\n", pid, child_count);
    fflush(stdout);
}

void list_children(void) {
    printf("Active children (%d):\n", child_count);
    for(int i = 0; i < child_count; i++) {
        printf("  %d: PID=%d\n", i+1, children[i]);
    }
    fflush(stdout);
}

void kill_all_children(void) {
    for(int i = 0; i < child_count; i++) {
        kill(children[i], SIGTERM);
        waitpid(children[i], NULL, 0);
    }
    child_count = 0;
    printf("All children killed\n");
    fflush(stdout);
}

int main(void) {
    setup_parent_signal_handlers();
    printf("Parent process started (PID=%d)\n", getpid());
    printf("Commands: + (create), - (kill last), l (list), k (kill all), q (quit)\n");
    fflush(stdout);

    while(1) {
        int cmd = getchar();
        if(cmd == EOF) continue;

        switch(cmd) {
            case '+': create_child(); break;
            case '-': kill_last_child(); break;
            case 'l': list_children(); break;
            case 'k': kill_all_children(); break;
            case 'q':
                kill_all_children();
                printf("Parent exiting\n");
                exit(EXIT_SUCCESS);
            case '\n': break; // Игнорируем символ новой строки
            default:
                printf("Unknown command: '%c'\n", cmd);
                fflush(stdout);
        }
    }
}