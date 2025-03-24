# Определяем компилятор и флаги
CC=gcc
CFLAGS=-Wall -Wextra -std=c11 -pedantic

# Определяем целевые файлы
PARENT=parent
CHILD=child
OBJECTS_PARENT=parent.o
OBJECTS_CHILD=child.o

# Правило по умолчанию
all: $(PARENT) $(CHILD)

# Правило для сборки родительского исполняемого файла
$(PARENT): $(OBJECTS_PARENT)
	$(CC) -o $@ $^

# Правило для сборки дочернего исполняемого файла
$(CHILD): $(OBJECTS_CHILD)
	$(CC) -o $@ $^

# Правило для компиляции объектов родительского процесса
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Чистка
clean:
	rm -f $(OBJECTS_PARENT) $(OBJECTS_CHILD) $(PARENT) $(CHILD)

# Установка переменной окружения CHILD_PATH (для документации)
set-env:
	@echo "Для запуска программы установите переменную окружения CHILD_PATH перед запуском."
	@echo "Пример: CHILD_PATH=$$(pwd) ./parent"
