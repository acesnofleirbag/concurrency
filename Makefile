CC = clang
CCFLAGS = -std=gnu99 -Wall -Werror -Wextra -pedantic

.PHONY: build
build: src/main.c
	$(CC) $(CCFLAGS) $^ -o build/server

serve:
	@./build/server

