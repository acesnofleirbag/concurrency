OBJ = $(shell find src -type f -iname '*.h' -or -iname '*.c')

CC = clang
CCFLAGS = -std=gnu99 -Wall -Werror -Wextra -pedantic -pthread
LDFLAGS = -lpthread -pthread

.PHONY: build
build: src/main.c
	$(CC) $(CCFLAGS) $^ -o build/server $(LDFLAGS)

serve:
	@./build/server

lint:
	@clang-format -style=file -i $(OBJ)
	@echo "reformatted successfully"
