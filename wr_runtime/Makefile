# WhiteRAILS Runtime Makefile
CC ?= gcc
# For Alpine, consider: CC = x86_64-linux-musl-gcc

SRCDIR := src
DEPDIR := deps
INCDIR := include
VPATH = $(SRCDIR):$(SRCDIR)/actions:$(DEPDIR)/cJSON

# Source files for the main runtime
# Note: With VPATH, we can simplify these to just the filenames
SRC := main.c \
       service_loader.c \
       condition.c \
       dispatcher.c \
       list_files.c \
       mkdir.c \
       run_command.c \
       notify.c \
       shell.c

# cJSON source (assuming it's compiled directly into the project)
SRC += cJSON.c

OBJ := $(SRC:.c=.o)

# CFLAGS for compilation
# -static: Link statically
# -Os: Optimize for size
# -Wall: Enable all warnings
# -Wextra: Enable extra warnings
# -pedantic: Issue all warnings demanded by strict ISO C
# -std=c11: Use the C11 standard
# -I$(INCDIR): Add include directory for our headers
# -I$(DEPDIR)/cJSON: Add include directory for cJSON header
# LDFLAGS for linking (used implicitly by linking .o files)
# -s: Strip all symbols from the output file (reduces size)
CFLAGS = -Os -Wall -Wextra -pedantic -std=c11 -Iinclude -Ideps/cJSON
LDFLAGS = -s

TARGET := wr_runtime

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) $(OBJ) -o $(TARGET) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(TARGET)

# Optional: A target to check compilation with a specific cross-compiler
# Example: make CC=x86_64-linux-musl-gcc
check-musl:
	$(MAKE) clean
	$(MAKE) CC=x86_64-linux-musl-gcc
