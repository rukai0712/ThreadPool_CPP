src_c = $(wildcard *.c)
target_c = $(patsubst %.c,%,$(src_c))

CC = gcc

ALL:$(target_c)

$(target_c):%:%.c
	$(CC) $< -o $@ -lpthread -Wall -g

clean:
	-rm -rf *.o $(target_c)

.PHONY: ALL clean
