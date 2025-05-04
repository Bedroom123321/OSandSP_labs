# Определяем компилятор и флаги
CC=gcc
CFLAGS=-Wall -Wextra -std=c11 -pedantic

# Определяем целевые файлы
PARENT1=parent1
PARENT2=parent2
OBJECTS_PARENT1=parent1.o
OBJECTS_PARENT2=parent2.o

# Правило по умолчанию
all: $(PARENT1) $(PARENT2)

# Правило для сборки родительского исполняемого файла
$(PARENT1): $(OBJECTS_PARENT1)
	$(CC) -o $@ $^

# Правило для сборки дочернего исполняемого файла
$(PARENT2): $(OBJECTS_PARENT2)
	$(CC) -o $@ $^

# Правило для компиляции объектов родительского процесса
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Чистка
clean:
	rm -f $(OBJECTS_PARENT1) $(OBJECTS_PARENT2) $(PARENT1) $(PARENT2)
