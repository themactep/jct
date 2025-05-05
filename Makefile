# Toolchain selection
# Override with: make CROSS_COMPILE=mipsel-linux-gnu-
CROSS_COMPILE ?=

# Default to native compilation
CC = $(CROSS_COMPILE)gcc
STRIP = $(CROSS_COMPILE)strip

# Flags
CFLAGS_BASE = -Wall -Wextra -std=c99 -pedantic -D_POSIX_C_SOURCE=200809L
CFLAGS = $(CFLAGS_BASE)
CFLAGS_RELEASE = $(CFLAGS_BASE) -Os -ffunction-sections -fdata-sections
LDFLAGS_BASE =
LDFLAGS = $(LDFLAGS_BASE)
LDFLAGS_RELEASE = $(LDFLAGS_BASE) -Wl,--gc-sections

# Directories and files
SRC_DIR = src
SOURCES = $(SRC_DIR)/json_value.c $(SRC_DIR)/json_parse.c $(SRC_DIR)/json_serialize.c $(SRC_DIR)/json_config.c $(SRC_DIR)/json_config_cli.c
OBJECTS = $(SOURCES:.c=.o)
TARGET = jct

.PHONY: all clean distclean release help

# Default target
all: $(TARGET)

# Help target
help:
	@echo "Available targets:"
	@echo "  make                  - Build regular version"
	@echo "  make release          - Build optimized version"
	@echo "  make clean            - Remove object files and executables"
	@echo "  make distclean        - Remove all generated files"
	@echo "  make help             - Show this help message"
	@echo ""
	@echo "Using CROSS_COMPILE:"
	@echo "  make CROSS_COMPILE=mipsel-linux-gnu-               - Use toolchain via PATH"
	@echo "  make CROSS_COMPILE=/path/to/toolchain/bin/prefix-  - Use any custom toolchain"

# Release builds with optimization
release: CFLAGS = $(CFLAGS_RELEASE)
release: LDFLAGS = $(LDFLAGS_RELEASE)
release: $(TARGET)
	$(STRIP) $(TARGET)

# Build rules
$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(SRC_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJECTS) $(TARGET) json_config_cli json_config_cli.mipsel

# Distclean removes all generated files, including object files, executables, and any temporary files
distclean: clean
	rm -f *~ src/*~ *.o src/*.o *.a *.so *.log *.out core core.*

# Dependencies
$(SRC_DIR)/json_value.o: $(SRC_DIR)/json_value.c $(SRC_DIR)/json_config.h
$(SRC_DIR)/json_parse.o: $(SRC_DIR)/json_parse.c $(SRC_DIR)/json_config.h
$(SRC_DIR)/json_serialize.o: $(SRC_DIR)/json_serialize.c $(SRC_DIR)/json_config.h
$(SRC_DIR)/json_config.o: $(SRC_DIR)/json_config.c $(SRC_DIR)/json_config.h
$(SRC_DIR)/json_config_cli.o: $(SRC_DIR)/json_config_cli.c $(SRC_DIR)/json_config.h
