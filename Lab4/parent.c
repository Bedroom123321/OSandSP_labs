#define _XOPEN_SOURCE 700
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <errno.h>

#define QUEUE_SIZE 10

// Ключи для shm и семафоров (в реальном коде лучше генерировать через ftok)
#define SHM_KEY 0x1234
#define SEM_KEY 0x5678

// Индексы семафоров в наборе
enum {
    SEM_MUTEX = 0,
    SEM_FILLCOUNT,
    SEM_EMPTYCOUNT,
    SEM_COUNT // общее количество семафоров
};

// --------------------------- Структуры -----------------------------

// Формат сообщения согласно условию (type, hash, size, data...)
typedef struct {
    unsigned char  type;    // offset 0 (1 байт)
    unsigned short hash;    // offset 1 (2 байта)
    unsigned char  size;    // offset 3 (1 байт: 0..256)
    // data начинается с offset=4, макс. 256 байт (выравнивание до кратности 4)
    unsigned char  data[256];
} message_t;

// Структура кольцевой очереди в разделяемой памяти
typedef struct {
    // собственно массив указателей/сообщений
    message_t buffer[QUEUE_SIZE];
    // индексы головы и хвоста
    int head;
    int tail;
    // Счётчики
    unsigned long producedCount;  // сколько всего добавлено
    unsigned long consumedCount;  // сколько всего извлечено
    // Количество активных производителей и потребителей (для отображения)
    int producers;
    int consumers;
} shm_data_t;

// ----------------------- Глобальные переменные ---------------------
// Эти переменные используются в дочерних процессах
static int shmid = -1;        // id сегмента разделяемой памяти
static int semid = -1;        // id набора семафоров
static shm_data_t *shm_ptr = NULL;  // Указатель на общую структуру в памяти

static volatile sig_atomic_t needTerminate = 0; // флаг завершения в детях

// ----------------------- Вспомогательные функции -------------------

// Обёртка для операции semop (упрощает код)
int sem_op(int sem_id, int sem_num, int op) {
    struct sembuf sb;
    sb.sem_num = sem_num;
    sb.sem_op  = op;
    sb.sem_flg = 0;
    return semop(sem_id, &sb, 1);
}

// Установить значение семафора
int sem_setval(int sem_id, int sem_num, int val) {
    union semun {
        int              val;
        struct semid_ds *buf;
        unsigned short  *array;
    } argument;
    argument.val = val;
    return semctl(sem_id, sem_num, SETVAL, argument);
}

// Простая функция вычисления "хэша" (контрольной суммы).
// Условие: "Значение поля hash при вычислении принимается равным нулю".
// Предположим, что нужно учесть байты: type, size и (size+1) байт из data.
// Здесь "size+1" подразумевает, что если size=0, то берём 1 байт из data.
unsigned short compute_hash(const message_t *msg) {
    unsigned short result = 0;
    // Сохраним старое значение hash, чтобы потом восстановить
    unsigned short old_hash = msg->hash;
    // Временно обнулим hash в копии
    message_t temp = *msg;
    temp.hash = 0;

    // Размер данных, который учитываем
    int data_len = (int)temp.size;
    if (data_len > 256) {
        data_len = 256; // на всякий случай
    }
    // Считаем сумму байтов type, size и data[0..data_len-1].
    // По условию "из байт сообщения длиной size + 1" (type+size+data)
    // Можно варьировать алгоритм, здесь простой XOR/сложение.
    result += temp.type;
    result += temp.size;

    for (int i = 0; i < data_len; i++) {
        result += temp.data[i];
    }

    // Вернём старое значение hash (чтобы не портить структуру извне)
    // хотя для вычисления не обязательно восстанавливать, но на всякий случай
    // если бы msg было не const.
    // (здесь msg - const, мы сделали копию, так что можно ничего не делать)
    (void)old_hash;

    return result;
}

int verify_hash(const message_t *msg) {
    unsigned short real_hash = compute_hash(msg);
    return (real_hash == msg->hash) ? 1 : 0;
}

void fill_random_message(message_t *msg) {
    // type — пусть будет случайный байт
    msg->type = (unsigned char)(rand() % 256);
    // size (0..255), но условие говорит, что реальный размер (1..256),
    // сделаем size в диапазоне 0..255, тогда итог "size+1" от 1..256
    msg->size = (unsigned char)(rand() % 256);

    // Заполним data случайными байтами (msg->size штук)
    for (int i = 0; i < msg->size; i++) {
        msg->data[i] = (unsigned char)(rand() % 256);
    }
    // Остальные байты обнулим
    for (int i = msg->size; i < 256; i++) {
        msg->data[i] = 0;
    }
    // Теперь вычислим hash
    msg->hash = 0; // временно 0
    unsigned short h = compute_hash(msg);
    msg->hash = h;
}


// Установим флаг, что нужно завершаться
void sig_handler(int signo) {
    (void)signo;
    needTerminate = 1;
}

void producer_loop(void) {
    srand((unsigned)time(NULL) ^ (unsigned)getpid());

    while (!needTerminate) {
        // Ждём, пока в очереди есть свободное место
        if (sem_op(semid, SEM_EMPTYCOUNT, -1) == -1) {
            if (errno == EINTR) continue; // сигнал, повторим
            break;
        }

        // Захватываем mutex
        if (sem_op(semid, SEM_MUTEX, -1) == -1) {
            break;
        }

        // Критическая секция: записываем в очередь
        int tail = shm_ptr->tail;
        // Заполним сообщение
        fill_random_message(&shm_ptr->buffer[tail]);

        // Сдвигаем хвост
        shm_ptr->tail = (tail + 1) % QUEUE_SIZE;
        // Увеличиваем счётчик добавленных
        shm_ptr->producedCount++;
        unsigned long producedNow = shm_ptr->producedCount;

        // Освобождаем mutex
        if (sem_op(semid, SEM_MUTEX, 1) == -1) {
            break;
        }

        // Увеличиваем fillCount
        if (sem_op(semid, SEM_FILLCOUNT, 1) == -1) {
            break;
        }

        // Выводим информацию
        printf("[Producer %d] Produced message #%lu (type=%u, size=%u)\n",
               getpid(), producedNow,
               (unsigned)shm_ptr->buffer[tail].type,
               (unsigned)shm_ptr->buffer[tail].size);
        fflush(stdout);

        // Небольшая задержка
        sleep(1);
    }

    // Завершаемся
    _exit(0);
}

void consumer_loop(void) {
    while (!needTerminate) {
        // Ждём, пока есть сообщения
        if (sem_op(semid, SEM_FILLCOUNT, -1) == -1) {
            if (errno == EINTR) continue; // сигнал, повторим
            break;
        }

        // Захватываем mutex
        if (sem_op(semid, SEM_MUTEX, -1) == -1) {
            break;
        }

        // Критическая секция: читаем из очереди
        int head = shm_ptr->head;
        message_t msg = shm_ptr->buffer[head]; // копируем локально
        shm_ptr->head = (head + 1) % QUEUE_SIZE;

        // Увеличиваем счётчик извлечённых
        shm_ptr->consumedCount++;
        unsigned long consumedNow = shm_ptr->consumedCount;

        // Освобождаем mutex
        if (sem_op(semid, SEM_MUTEX, 1) == -1) {
            break;
        }

        // Освобождаем место (emptyCount++)
        if (sem_op(semid, SEM_EMPTYCOUNT, 1) == -1) {
            break;
        }

        // Проверяем hash
        int ok = verify_hash(&msg);
        printf("[Consumer %d] Consumed message #%lu (type=%u, size=%u, hash_ok=%s)\n",
               getpid(), consumedNow, (unsigned)msg.type,
               (unsigned)msg.size, ok ? "YES" : "NO");
        fflush(stdout);

        // Небольшая задержка
        sleep(1);
    }

    _exit(0);
}

int main(void) {
    // Установим обработчик сигналов (например, SIGINT для главного процесса).
    signal(SIGINT, SIG_IGN); 

    // Создадим набор семафоров
    semid = semget(SEM_KEY, SEM_COUNT, IPC_CREAT | 0666);
    if (semid < 0) {
        perror("semget");
        exit(1);
    }

    // Инициализируем семафоры
    if (sem_setval(semid, SEM_MUTEX, 1) < 0) {
        perror("semctl(mutex)");
        exit(1);
    }
    if (sem_setval(semid, SEM_FILLCOUNT, 0) < 0) {
        perror("semctl(fillcount)");
        exit(1);
    }
    if (sem_setval(semid, SEM_EMPTYCOUNT, QUEUE_SIZE) < 0) {
        perror("semctl(emptycount)");
        exit(1);
    }

    // Создадим разделяемую память
    shmid = shmget(SHM_KEY, sizeof(shm_data_t), IPC_CREAT | 0666);
    if (shmid < 0) {
        perror("shmget");
        exit(1);
    }

    // Подключим сегмент
    shm_ptr = (shm_data_t*)shmat(shmid, NULL, 0);
    if (shm_ptr == (void*)-1) {
        perror("shmat");
        exit(1);
    }

    // Инициализируем данные в shared memory
    shm_ptr->head = 0;
    shm_ptr->tail = 0;
    shm_ptr->producedCount = 0;
    shm_ptr->consumedCount = 0;
    shm_ptr->producers = 0;
    shm_ptr->consumers = 0;

    // Списки PID-ов производителей и потребителей (для завершения)
    // Можно сделать массив фиксированной длины или динамический.
    pid_t producers_pid[100];
    pid_t consumers_pid[100];
    int pCount = 0;
    int cCount = 0;

    printf("Press:\n"
           "  p - spawn producer\n"
           "  c - spawn consumer\n"
           "  P - kill one producer\n"
           "  C - kill one consumer\n"
           "  s - show status\n"
           "  q - quit\n");

    // Основной цикл чтения команд
    while (1) {
        int ch = getchar();
        if (ch == EOF) {
            // Может быть, Ctrl+D или что-то такое
            break;
        }
        if (ch == '\n') {
            continue;
        }

        if (ch == 'p') {
            // Создаём производителя
            pid_t pid = fork();
            if (pid < 0) {
                perror("fork producer");
            } else if (pid == 0) {
                // Дочерний процесс (producer)
                // Установим свой обработчик сигнала SIGTERM/SIGINT
                signal(SIGINT, sig_handler);
                signal(SIGTERM, sig_handler);
                producer_loop(); // не возвращается
            } else {
                // Родитель
                producers_pid[pCount++] = pid;
                shm_ptr->producers++;
                printf("Spawned producer PID=%d\n", pid);
            }
        } else if (ch == 'c') {
            // Создаём потребителя
            pid_t pid = fork();
            if (pid < 0) {
                perror("fork consumer");
            } else if (pid == 0) {
                // Дочерний процесс (consumer)
                signal(SIGINT, sig_handler);
                signal(SIGTERM, sig_handler);
                consumer_loop(); // не возвращается
            } else {
                // Родитель
                consumers_pid[cCount++] = pid;
                shm_ptr->consumers++;
                printf("Spawned consumer PID=%d\n", pid);
            }
        } else if (ch == 'P') {
            // Завершить одного производителя
            if (pCount > 0) {
                pid_t killPid = producers_pid[pCount - 1];
                kill(killPid, SIGTERM);
                // подождём, пока он завершится
                waitpid(killPid, NULL, 0);
                printf("Killed producer PID=%d\n", killPid);
                pCount--;
                shm_ptr->producers--;
            } else {
                printf("No producers to kill.\n");
            }
        } else if (ch == 'C') {
            // Завершить одного потребителя
            if (cCount > 0) {
                pid_t killPid = consumers_pid[cCount - 1];
                kill(killPid, SIGTERM);
                waitpid(killPid, NULL, 0);
                printf("Killed consumer PID=%d\n", killPid);
                cCount--;
                shm_ptr->consumers--;
            } else {
                printf("No consumers to kill.\n");
            }
        } else if (ch == 's') {
            // Показать состояние
            int head = shm_ptr->head;
            int tail = shm_ptr->tail;
            int used = (tail >= head) ? (tail - head) : (QUEUE_SIZE - head + tail);
            int free_slots = QUEUE_SIZE - used;
            printf("Queue status:\n");
            printf("  producers: %d\n", shm_ptr->producers);
            printf("  consumers: %d\n", shm_ptr->consumers);
            printf("  producedCount: %lu\n", shm_ptr->producedCount);
            printf("  consumedCount: %lu\n", shm_ptr->consumedCount);
            printf("  queue used: %d\n", used);
            printf("  queue free: %d\n", free_slots);
        } else if (ch == 'q') {
            // Выходим
            break;
        } else {
            printf("Unknown command '%c'\n", ch);
        }
    }

    // Завершение: убьём всех оставшихся производителей/потребителей
    for (int i = 0; i < pCount; i++) {
        kill(producers_pid[i], SIGTERM);
    }
    for (int i = 0; i < cCount; i++) {
        kill(consumers_pid[i], SIGTERM);
    }

    // Дождёмся завершения всех
    for (int i = 0; i < pCount; i++) {
        waitpid(producers_pid[i], NULL, 0);
    }
    for (int i = 0; i < cCount; i++) {
        waitpid(consumers_pid[i], NULL, 0);
    }

    // Удалим семафоры
    if (semctl(semid, 0, IPC_RMID, 0) < 0) {
        perror("semctl(IPC_RMID)");
    }

    // Отключим и удалим разделяемую память
    if (shmdt(shm_ptr) < 0) {
        perror("shmdt");
    }
    if (shmctl(shmid, IPC_RMID, NULL) < 0) {
        perror("shmctl(IPC_RMID)");
    }

    printf("Main process exiting.\n");
    return 0;
}
