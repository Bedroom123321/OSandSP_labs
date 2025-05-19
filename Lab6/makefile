CC = gcc
CFLAGS = -W -Wall -Wextra -std=c11 -pedantic
LDFLAGS = -pthread

all: sort_index gen view

sort_index: sort_index.c
	$(CC) $(CFLAGS) -o sort_index sort_index.c $(LDFLAGS)

gen: gen.c
	$(CC) $(CFLAGS) -o gen gen.c

view: view.c
	$(CC) $(CFLAGS) -o view view.c

clean:
	rm -f sort_index gen view tmp.sorted

.PHONY: all clean