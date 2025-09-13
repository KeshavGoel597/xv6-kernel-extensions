
# Makefile for POSIX-compliant shell

CC     = gcc
CFLAGS = -std=c99 \
         -D_POSIX_C_SOURCE=200809L \
         -D_XOPEN_SOURCE=700 \
         -Wall -Wextra -Werror \
         -Wno-unused-parameter \
         -Wno-unused-variable \
         -fno-asm -g -Iinclude
LDFLAGS =

SRCDIR = src
INCDIR = include
OBJDIR = obj

SRC    = $(wildcard $(SRCDIR)/*.c)
OBJ    = $(SRC:$(SRCDIR)/%.c=$(OBJDIR)/%.o)
OUT    = shell.out 

.PHONY: all clean

all: $(OUT)

$(OUT): $(OBJ) | $(OBJDIR)
	$(CC) $(OBJ) -o $(OUT) $(LDFLAGS)

$(OBJDIR)/%.o: $(SRCDIR)/%.c $(INCDIR)/definition.h | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR):
	mkdir -p $(OBJDIR)

clean:
	rm -rf $(OUT) $(OBJDIR)
