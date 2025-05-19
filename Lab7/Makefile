# Определяем компилятор и флаги
CC=gcc
CFLAGS=-Wall -Wextra -std=c11 -pedantic

# Определяем целевые файлы
MAIN=main
OBJECTS_MAIN=main.o

all: $(MAIN)  # Change $(PARENT) to $(MAIN)

# Правило для сборки исполняемого файла
$(MAIN): $(OBJECTS_MAIN)
	$(CC) -o $@ $^

# Правило для компиляции объектов
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Чистка
clean:
	rm -f $(OBJECTS_MAIN) $(MAIN)