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
        return;
    }

    // Считываем имена файлов
    while ((entry = readdir(dir)) != NULL) {
        // Пропускаем специальные записи "." и ".."
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        struct stat file_stat;
        char full_path[1024];

        snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);

        // Получаем информацию о файле
        if (lstat(full_path, &file_stat) == -1) {
            perror("stat");
            continue;
        }

        int is_symlink = S_ISLNK(file_stat.st_mode);
        int is_dir = S_ISDIR(file_stat.st_mode);
        int is_file = S_ISREG(file_stat.st_mode);

        // Проверка фильтров
        if ((list_dirs && is_dir) || (list_files && is_file) || (list_symlinks && is_symlink)) {
            // Сохраняем имя файла
            names = realloc(names, sizeof(char *) * (count + 1));
            names[count] = strdup(full_path);
            count++;
        }

        //Рекурсия
        if (is_dir && list_dirs) {
            scan(full_path, list_symlinks, list_dirs, list_files, sort);
        }

    }
    closedir(dir);

    // Сортируем имена, если указано
    if (sort) {
        setlocale(LC_COLLATE, "");
        qsort(names, count, sizeof(char *), compare);
    }

    // Выводим имена файлов
    for (size_t i = 0; i < count; i++) {
        if (names[i] != NULL) {
            printf("%s\n", names[i]); // Выводим только действительные имена
            free(names[i]); // Освобождаем память
        }
    }
    free(names); // Освобождаем массив

}