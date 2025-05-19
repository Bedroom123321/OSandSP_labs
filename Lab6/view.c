#include <stdio.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>

struct index_s {
    double time_mark;
    uint64_t recno;
};

struct index_hdr_s {
    uint64_t records;
    struct index_s idx[];
};

int main(int argc, char* argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s filename\n", argv[0]);
        return 1;
    }

    int fd = open(argv[1], O_RDONLY);
    if (fd == -1) {
        perror("open");
        return 1;
    }

    uint64_t records;
    read(fd, &records, sizeof(uint64_t));
    printf("Records: %lu\n", records);

    struct index_s rec;
    for (uint64_t i = 0; i < records; i++) {
        read(fd, &rec, sizeof(struct index_s));
        printf("time_mark: %.6f, recno: %lu\n", rec.time_mark, rec.recno);
    }

    close(fd);
    return 0;
}