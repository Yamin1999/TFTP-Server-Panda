# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -g -Iinclude -MMD -MP
LDFLAGS = -lws2_32

# Directories
SRC_DIR = src
OBJ_DIR = obj
RES_DIR = res

# Source and object files
SRCS = $(wildcard $(SRC_DIR)/*.c)
OBJS = $(SRCS:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)
DEPS = $(OBJS:.o=.d)

# Resource files and their objects
RES_FILES = $(RES_DIR)/tftpd_icon.rc $(RES_DIR)/tftpd_version.rc
RES_OBJS = $(RES_FILES:$(RES_DIR)/%.rc=$(OBJ_DIR)/%.o)

# Target executable
TARGET = Panda

# Default target
all: $(TARGET)

# Linking the target with resource objects
$(TARGET): $(OBJS) $(RES_OBJS)
	$(CC) $(OBJS) $(RES_OBJS) -o $@ $(LDFLAGS)

# Compiling object files
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Compiling resource files
$(OBJ_DIR)/%.o: $(RES_DIR)/%.rc | $(OBJ_DIR)
	windres $< -o $@

# Creating object directory if it doesn't exist
$(OBJ_DIR):
	mkdir $@

# Clean up generated files
clean:
	@if exist $(OBJ_DIR) rmdir /S /Q $(OBJ_DIR)
	@if exist $(TARGET).exe del /Q $(TARGET).exe
	@if exist $(TARGET) del /Q $(TARGET)
	@for %%f in ($(DEPS)) do if exist %%f del /Q %%f
	@for %%f in ($(RES_OBJS)) do if exist %%f del /Q %%f
realclean: clean
	@if exist $(OBJ_DIR) rmdir /S /Q $(OBJ_DIR)
	@if exist $(TARGET).exe del /Q $(TARGET).exe
	@if exist $(TARGET) del /Q $(TARGET)
	@for %%f in ($(DEPS)) do if exist %%f del /Q %%f
	@for %%f in ($(RES_OBJS)) do if exist %%f del /Q %%f

# Include dependency files
-include $(DEPS)

# Phony targets
.PHONY: all clean realclean
