CC = gcc
CFLAGS = -O3 -Wall -Wextra
LIBS = -lpthread
DEPS =
OBJ = iombench.o
PROGRAMS = iombench

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

iombench: $(OBJ)
	gcc -o $@ $^ $(CFLAGS) $(LIBS)

.PHONY: clean

clean:
	rm -f $(OBJ) $(PROGRAMS)

