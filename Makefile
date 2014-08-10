CC = gcc
CFLAGS = -O3 -Wall -Wextra
LIBS = -lpthread
DEPS =
OBJ = iobench.o
PROGRAMS = iobench

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

iobench: $(OBJ)
	gcc -o $@ $^ $(CFLAGS) $(LIBS)

.PHONY: clean

clean:
	rm -f $(OBJ) $(PROGRAMS)

