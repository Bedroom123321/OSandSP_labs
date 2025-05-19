#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>

#define FILE_NAME "students.dat"
#define MAX_NAME 80
#define MAX_ADDRESS 80

struct record_s {
    char name[MAX_NAME];
    char address[MAX_ADDRESS];
    uint8_t semester;
};

void init_file();
void list_records();
void get_record(int rec_no, struct record_s *rec);
void put_record(int rec_no, struct record_s *rec);
void lock_record(int fd, int rec_no);
void unlock_record(int fd, int rec_no);
void menu();
int get_record_count();

void init_file() {
    struct record_s records[] = {
            {"Ivanov Ivan Ivanovich", "Moscow, Lenina 1", 1},
            {"Petrov Petr Petrovich", "St. Petersburg, Nevsky 10", 2},
            {"Sidorov Sidr Sidorovich", "Kazan, Bauman 5", 3},
            {"Smirnov Alexey Viktorovich", "Novosibirsk, Lenin 15", 4},
            {"Kuznetsov Dmitry Sergeevich", "Ekaterinburg, Mira 20", 5},
            {"Popov Vladimir Alexandrovich", "Samara, Soviet 25", 6},
            {"Vasiliev Sergey Mikhailovich", "Rostov, Bolshaya 30", 7},
            {"Mikhailov Mikhail Pavlovich", "Ufa, Lenin 35", 8},
            {"Fedorov Fedor Fedorovich", "Omsk, Marx 40", 9},
            {"Morozov Nikita Igorevich", "Chelyabinsk, Kirov 45", 10}
    };

    int fd = open(FILE_NAME, O_CREAT | O_WRONLY | O_TRUNC, 0644);
    if (fd == -1) {
        perror("Error creating file");
        exit(1);
    }

    for (int i = 0; i < 10; i++) {
        if (write(fd, &records[i], sizeof(struct record_s)) != sizeof(struct record_s)) {
            perror("Error writing to file");
            close(fd);
            exit(1);
        }
    }
    close(fd);
}

int get_record_count() {
    int fd = open(FILE_NAME, O_RDONLY);
    if (fd == -1) return 0;

    off_t size = lseek(fd, 0, SEEK_END);
    close(fd);
    return size / sizeof(struct record_s);
}

void list_records() {
    struct record_s rec;
    int fd = open(FILE_NAME, O_RDONLY);
    if (fd == -1) {
        perror("Error opening file");
        return;
    }

    printf("\nRecords in file:\n");
    for (int i = 0; i < get_record_count(); i++) {
        lseek(fd, i * sizeof(struct record_s), SEEK_SET);
        if (read(fd, &rec, sizeof(struct record_s)) == sizeof(struct record_s)) {
            printf("%d. Name: %s, Address: %s, Semester: %d\n",
                   i + 1, rec.name, rec.address, rec.semester);
        }
    }
    close(fd);
}

void get_record(int rec_no, struct record_s *rec) {
    int fd = open(FILE_NAME, O_RDONLY);
    if (fd == -1) {
        perror("Error opening file");
        return;
    }

    lseek(fd, (rec_no - 1) * sizeof(struct record_s), SEEK_SET);
    if (read(fd, rec, sizeof(struct record_s)) != sizeof(struct record_s)) {
        perror("Error reading record");
    }
    close(fd);
}

void put_record(int rec_no, struct record_s *rec) {
    int fd = open(FILE_NAME, O_WRONLY);
    if (fd == -1) {
        perror("Error opening file");
        return;
    }

    lseek(fd, (rec_no - 1) * sizeof(struct record_s), SEEK_SET);
    if (write(fd, rec, sizeof(struct record_s)) != sizeof(struct record_s)) {
        perror("Error writing record");
    }
    close(fd);
}

void lock_record(int fd, int rec_no) {
    struct flock lock;
    memset(&lock, 0, sizeof(lock));
    lock.l_type = F_WRLCK;
    lock.l_whence = SEEK_SET;
    lock.l_start = (rec_no - 1) * sizeof(struct record_s);
    lock.l_len = sizeof(struct record_s);

    if (fcntl(fd, F_SETLKW, &lock) == -1) {
        perror("Error locking record");
        exit(1);
    }
}

void unlock_record(int fd, int rec_no) {
    struct flock lock;
    memset(&lock, 0, sizeof(lock));
    lock.l_type = F_UNLCK;
    lock.l_whence = SEEK_SET;
    lock.l_start = (rec_no - 1) * sizeof(struct record_s);
    lock.l_len = sizeof(struct record_s);

    if (fcntl(fd, F_SETLK, &lock) == -1) {
        perror("Error unlocking record");
        exit(1);
    }
}

void modify_and_save_record(int rec_no) {
    struct record_s rec, rec_wrk, rec_new;
    int fd = open(FILE_NAME, O_RDWR);
    if (fd == -1) {
        perror("Error opening file");
        return;
    }

    Again:
    get_record(rec_no, &rec);
    rec_wrk = rec;

    printf("\nCurrent record:\nName: %s\nAddress: %s\nSemester: %d\n",
           rec_wrk.name, rec_wrk.address, rec_wrk.semester);
    printf("Enter new name: ");
    fgets(rec_wrk.name, MAX_NAME, stdin);
    rec_wrk.name[strcspn(rec_wrk.name, "\n")] = 0;
    printf("Enter new address: ");
    fgets(rec_wrk.address, MAX_ADDRESS, stdin);
    rec_wrk.address[strcspn(rec_wrk.address, "\n")] = 0;
    printf("Enter new semester: ");
    scanf("%hhu", &rec_wrk.semester);
    while (getchar() != '\n');

    if (memcmp(&rec_wrk, &rec, sizeof(struct record_s)) != 0) {
        lock_record(fd, rec_no);
        get_record(rec_no, &rec_new);

        if (memcmp(&rec_new, &rec, sizeof(struct record_s)) != 0) {
            unlock_record(fd, rec_no);
            printf("Record was modified by another process!\n");
            rec = rec_new;
            goto Again;
        }

        put_record(rec_no, &rec_wrk);
        unlock_record(fd, rec_no);
        printf("Record successfully updated\n");
    }
    close(fd);
}

void menu() {
    int choice, rec_no;
    struct record_s rec;

    while (1) {
        printf("\nMenu:\n");
        printf("1. LST - List all records\n");
        printf("2. GET - Get record by number\n");
        printf("3. MODIFY - Modify and save record\n");
        printf("4. Exit\n");
        printf("Enter choice: ");
        scanf("%d", &choice);
        while (getchar() != '\n');

        switch (choice) {
            case 1:
                list_records();
                break;
            case 2:
                printf("Enter record number (1-%d): ", get_record_count());
                scanf("%d", &rec_no);
                while (getchar() != '\n');
                if (rec_no >= 1 && rec_no <= get_record_count()) {
                    get_record(rec_no, &rec);
                    printf("Record %d:\nName: %s\nAddress: %s\nSemester: %d\n",
                           rec_no, rec.name, rec.address, rec.semester);
                } else {
                    printf("Invalid record number\n");
                }
                break;
            case 3:
                printf("Enter record number to modify (1-%d): ", get_record_count());
                scanf("%d", &rec_no);
                while (getchar() != '\n');
                if (rec_no >= 1 && rec_no <= get_record_count()) {
                    modify_and_save_record(rec_no);
                } else {
                    printf("Invalid record number\n");
                }
                break;
            case 4:
                return;
            default:
                printf("Invalid choice\n");
        }
    }
}

int main() {
    if (access(FILE_NAME, F_OK) == -1) {
        init_file();
    }

    menu();
    return 0;
}