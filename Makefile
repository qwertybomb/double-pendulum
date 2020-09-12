cc = clang
flags = -Ofast -std=gnu99 -Wall
linker_flags = -lmingw32 -lSDL2main -lSDL2

all: main.c
	$(cc) $(flags) main.c -o main $(linker_flags)
