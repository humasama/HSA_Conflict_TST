CC=gcc
PGMS=mc-mapping
SRC_FILES = mc-mapping.c
CFLAGS=-Wall

all: $(PGMS)
$(PGMS): $(SRC_FILES)
	$(CC) $< -o $@ -lrt -g -O0

.PHONY: clean
clean:
	rm $(wildcard *.o) $(wildcard  *~) $(PGMS)
