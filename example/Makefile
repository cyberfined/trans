CC=gcc
CFLAGS=-std=c11 -Wall -O0 -g
LDFLAGS=
SRC=main.c lang.c
OBJ=$(patsubst %.c, %.o, $(SRC))
TARGET=example
.PHONY: all clean
all: $(TARGET)
$(TARGET): $(OBJ)
	$(CC) $(LDFLAGS) $(OBJ) -o $(TARGET)
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@
clean:
	rm -f $(OBJ) $(TARGET)
