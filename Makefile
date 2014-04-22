CC = gcc

ifdef DEBUG
C_FLAGS = -std=c99 -Wall -Wextra -g -O0
LINK_FLAG = -O0 -g
else
C_FLAGS = -std=c99 -Wall -Wextra -O3
endif


C_SOURCES=$(shell find src -name *.c)
C_OBJECTS=$(C_SOURCES:src/%.c=build/%.o)
INCLUDE_DIR = src/include


## link
pr4: $(C_OBJECTS)
	$(CC) $(LINK_FLAG) $^ -o $@

## build sources individually
build/%.o: src/%.c
	$(CC) $(C_FLAGS) -c -I $(INCLUDE_DIR) $< -o $@

clean:
	rm -f pr4
	rm -f build/*


