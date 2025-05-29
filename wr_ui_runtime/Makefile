# Makefile for wr_ui_runtime

# Compiler and Linker
CC = gcc

# Source files
# Assuming cJSON.c is located at ../wr_runtime/deps/cJSON/cJSON.c
# Adjust CJSON_SRC path if it's different or if cJSON is installed system-wide
CJSON_DIR = ../wr_runtime/deps/cJSON
C_SOURCES = wr_ui_runtime.c $(CJSON_DIR)/cJSON.c

# Target executable
TARGET = wr_ui_runtime

# Compiler flags
# -Wall: Enable all warnings
# -g: Add debugging information
# $(shell pkg-config --cflags gtk+-3.0 webkit2gtk-4.0): Get include paths for GTK3 and WebKit2GTK
# -I$(CJSON_DIR): Add include path for our local cJSON header
CFLAGS = -Wall -g $(shell pkg-config --cflags gtk+-3.0 webkit2gtk-4.0) -I$(CJSON_DIR) -Wno-deprecated-declarations

# Linker flags
# $(shell pkg-config --libs gtk+-3.0 webkit2gtk-4.0): Get library paths and libraries for GTK3 and WebKit2GTK
LDFLAGS = $(shell pkg-config --libs gtk+-3.0 webkit2gtk-4.0)

# Object files: create .o files for each .c file in the current directory
# For cJSON, we'll compile it directly into the main target to keep it simple,
# or we could create a cJSON.o object file separately.
# For simplicity here, cJSON.c is added to C_SOURCES and will be compiled with wr_ui_runtime.c
# OBJECTS = $(patsubst %.c,%.o,$(filter %.c,$(C_SOURCES))) # Only .c files in current dir - Not used in simple rule
# CJSON_OBJ = $(CJSON_DIR)/cJSON.o # Not used in simple rule


# Default target: build all
all: $(TARGET)

# Target to build the executable
# This rule compiles all source files listed in C_SOURCES directly into the target executable.
$(TARGET): $(C_SOURCES)
	$(CC) $(CFLAGS) $^ -o $(TARGET) $(LDFLAGS)

# Target to clean up build files
clean:
	rm -f $(TARGET) *.o
	@# If cJSON.o was built separately in its own directory, this would be more complex.
	@# Since we compile cJSON.c directly with wr_ui_runtime.c in a single command, 
	@# there's no separate cJSON.o in this directory or in CJSON_DIR that this clean rule targets.
	@# To clean a cJSON.o in CJSON_DIR, you would add: rm -f $(CJSON_DIR)/cJSON.o
	@echo "Cleaned."

.PHONY: all clean
