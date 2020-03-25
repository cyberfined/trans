CC=gcc
CFLAGS=-std=c11 -Wall -O3
LDFLAGS=
SRC=main.c regexp.c trans.c llist.c htable.c
OBJ=$(patsubst %.c, %.o, $(SRC))
TARGET=trans
.PHONY: all clean
all: $(TARGET)
$(TARGET): $(OBJ)
	$(CC) $(LDFLAGS) $(OBJ) -o $(TARGET)
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@
clean:
	rm -f $(OBJ) $(TARGET)
