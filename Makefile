CC = clang
CCFLAGS = -std=gnu99 -Wall -Werror -Wextra -pedantic -pthread
LDFLAGS = -lpthread -pthread

.PHONY: build
build: src/main.c
	$(CC) $(CCFLAGS) $^ -o build/server $(LDFLAGS)

serve:
	@./build/server

