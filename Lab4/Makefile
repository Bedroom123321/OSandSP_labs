# Определяем компилятор и флаги
CC=gcc
CFLAGS=-Wall -Wextra -std=c11 -pedantic

# Определяем целевые файлы
PARENT=parent
OBJECTS_PARENT=parent.o

all: $(PARENT)

# Правило для сборки родительского исполняемого файла
$(PARENT): $(OBJECTS_PARENT)
	$(CC) -o $@ $^

# Правило для компиляции объектов родительского процесса
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Чистка
clean:
	rm -f $(OBJECTS_PARENT) $(PARENT)

