CC = gcc

# we ignore unused parameters because of the way pr4.c dispatches functions. some 
# actions inherently ignore any possible arguments given to them, so the parameters are unused.

ifdef DEBUG
C_FLAGS = -std=c99 -Wall -Wextra -g -O0 -Wno-unused-parameter
LINK_FLAG = -O0 -g
else
C_FLAGS = -std=c99 -Wall -Wextra -O3 -Wno-unused-parameter
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

clean-all: clean
	rm -f *.data


