#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>


void print_usage() {
    printf("Usage: dirwalk [dir] [options]\n");
    printf("Options:\n");
    printf("  -l          list symbolic links only\n");
    printf("  -d          list directories\n");
    printf("  -f          list files\n");
    printf("  -s          sort list\n");
}

extern void scan(const char *path, int list_symlinks, int list_dirs, int list_files, int sort);

int main(int argc, char *argv[]) {
    int only_symlinks_flag = 0;
    int only_dirs_flag = 0;
    int only_files_flag = 0;
    int sort_flag = 0;
    int option;

    while ((option = getopt(argc, argv, "ldfs")) != -1) {
        switch (option) {
            case 'l':
                only_symlinks_flag = 1; // флаг символические ссылки
                break;
            case 'd':
                only_dirs_flag = 1; // флаг только каталоги
                break;
            case 'f':
                only_files_flag = 1; // флаг только файлы
                break;
            case 's':
                sort_flag = 1; // флаг сортировки
                break;
            default:
                print_usage();
                exit(EXIT_FAILURE);
        }
    }

    const char *dir = (optind < argc) ? argv[optind] : "/";

    scan(dir, only_symlinks_flag, only_dirs_flag, only_files_flag,  sort_flag);
    return 0;
}