#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <string.h>
#include <sys/stat.h>
#include <locale.h>

int compare(const void *a, const void *b) {
    return strcmp(*(const char **)a, *(const char **)b);
}

void scan(const char *path, int list_symlinks, int list_dirs, int list_files, int sort) {
    struct dirent *entry;
    DIR *dir = opendir(path);
    char **names = NULL;
    size_t count = 0;

    if (dir == NULL) {
        perror("opendir");
        return;
    }

    // Считываем имена файлов
    while ((entry = readdir(dir)) != NULL) {
        struct stat file_stat;
        char full_path[1024];

        snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);

        // Получаем информацию о файле
        if (lstat(full_path, &file_stat) == -1) {
            perror("stat");
            continue;
        }

        int is_dir = S_ISDIR(file_stat.st_mode);
        int is_symlink = S_ISLNK(file_stat.st_mode);
        int is_file = S_ISREG(file_stat.st_mode);

        // Проверка фильтров
        if ((list_dirs && !is_dir) || (list_files && !is_file) || (list_symlinks && !is_symlink)) {
            continue;
        }

        // Сохраняем имя файла
        names = realloc(names, sizeof(char *) * (count + 1));
        names[count] = strdup(entry->d_name);
        count++;
    }
    closedir(dir);

    // Сортируем имена в соответствии с LC_COLLATE, если указано
    if (sort) {
        setlocale(LC_COLLATE, "");
        qsort(names, count, sizeof(char *), compare);
    }

    // Выводим имена файлов
    for (size_t i = 0; i < count; i++) {
        printf("%s\n", names[i]);
        free(names[i]); // Освобождаем память
    }
    printf("\n");
    free(names); // Освобождаем массив
}