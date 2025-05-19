#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
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
    if (argc != 3) {
        fprintf(stderr, "Usage: %s records filename\n", argv[0]);
        return 1;
    }

    uint64_t records = atoll(argv[1]);
    const char* filename = argv[2];

    if (records % 256 != 0) {
        fprintf(stderr, "records must be multiple of 256\n");
        return 1;
    }

    int fd = open(filename, O_CREAT | O_WRONLY | O_TRUNC, 0666);
    if (fd == -1) {
        perror("open");
        return 1;
    }

    write(fd, &records, sizeof(uint64_t));
    srand(time(NULL));
    double mjd_min = 15020.0;
    double mjd_max = 2460299.0;
    for (uint64_t i = 0; i < records; i++) {
        struct index_s rec;
        double integer_part = mjd_min + (rand() / (double)RAND_MAX) * (mjd_max - mjd_min);
        double fractional_part = (rand() / (double)RAND_MAX);
        rec.time_mark = integer_part + fractional_part;
        rec.recno = i + 1;
        write(fd, &rec, sizeof(struct index_s));
    }

    close(fd);
    return 0;
}