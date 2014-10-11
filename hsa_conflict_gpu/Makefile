LFLAGS := -Wl,--unresolved-symbols=ignore-in-shared-libs
CFLAGS := -g

CC := gcc
OBJ_DIR := objs
TARGET_DIR := .

LIBS := -L../../lib/x86_64 -lhsa-runtime64 -lelf

HSA_HEAD := -I../../include 
CONFLICT_HEAD := -I./include

SRC_FILES := $(wildcard *.c)
OBJ_FILES := $(SRC_FILES:.c=.o)
TARGET_FILE := hsa_conflict

OBJECT := $(addprefix $(OBJ_DIR)/, $(OBJ_FILES))
TARGET := $(addprefix $(TARGET_DIR)/, $(TARGET_FILE))

#vpath %.o $(OBJ_DIR)

all: $(OBJ_DIR) $(TARGET)

$(TARGET): $(OBJECT) | $(TARGET_DIR) $(OBJ_DIR)
	$(CC) $(OBJECT) -o $@  $(LIBS) -pg $(CFLAGS)

$(OBJ_DIR):
	mkdir $(OBJ_DIR)

$(OBJ_DIR)/%o: %c
	$(CC) $(CFLAGS) $(HSA_HEAD) $(CONFLICT_HEAD) -c $< -o $@ -pg -std=c99


.PHONY: clean
clean: 
	rm -rf $(TARGET) $(OBJECT)

