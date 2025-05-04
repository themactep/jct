# Toolchain selection
# Available options:
# - empty: use native compiler
# - mipsel-linux-gnu-: use system GNU MIPS toolchain
# - /path/to/toolchain/bin/mipsel-linux-musl-: use custom toolchain
# Override with: make CROSS_COMPILE=mipsel-linux-gnu- mipsel
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
TARGET_MIPSEL = jct.mipsel

.PHONY: all clean distclean native mipsel release release-mipsel help

# Default target
all: native

# Help target
help:
	@echo "Available targets:"
	@echo "  make                  - Build native version"
	@echo "  make native           - Build native version"
	@echo "  make mipsel           - Build MIPS version using selected toolchain"
	@echo "  make release          - Build optimized native version"
	@echo "  make release-mipsel   - Build optimized MIPS version"
	@echo "  make clean            - Remove object files and executables"
	@echo "  make distclean        - Remove all generated files"
	@echo "  make help             - Show this help message"
	@echo ""
	@echo "Toolchain selection (using CROSS_COMPILE):"
	@echo "  make mipsel                                              - Use system GNU MIPS toolchain by default"
	@echo "  make CROSS_COMPILE=mipsel-linux-gnu- mipsel              - Use system GNU MIPS toolchain explicitly"
	@echo "  make CROSS_COMPILE=/path/to/toolchain/bin/prefix- mipsel - Use any custom toolchain"
	@echo ""
	@echo "MIPS compiler: $(CC_MIPSEL)"

# Native compilation
native: $(TARGET)

# MIPS cross-compilation
mipsel: $(TARGET_MIPSEL)

# Release builds with optimization
release: CFLAGS = $(CFLAGS_RELEASE)
release: LDFLAGS = $(LDFLAGS_RELEASE)
release: $(TARGET)
	$(STRIP) $(TARGET)

# MIPS release build
release-mipsel: CFLAGS = $(CFLAGS_RELEASE)
release-mipsel: LDFLAGS = $(LDFLAGS_RELEASE)
release-mipsel: $(TARGET_MIPSEL)
	$(STRIP) $(TARGET_MIPSEL)

# Build rules
$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(TARGET_MIPSEL): $(OBJECTS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(SRC_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJECTS) $(TARGET) $(TARGET_MIPSEL) json_config_cli json_config_cli.mipsel

# Distclean removes all generated files, including object files, executables, and any temporary files
distclean: clean
	rm -f *~ src/*~ *.o src/*.o *.a *.so *.log *.out core core.*

# Dependencies
$(SRC_DIR)/json_value.o: $(SRC_DIR)/json_value.c $(SRC_DIR)/json_config.h
$(SRC_DIR)/json_parse.o: $(SRC_DIR)/json_parse.c $(SRC_DIR)/json_config.h
$(SRC_DIR)/json_serialize.o: $(SRC_DIR)/json_serialize.c $(SRC_DIR)/json_config.h
$(SRC_DIR)/json_config.o: $(SRC_DIR)/json_config.c $(SRC_DIR)/json_config.h
$(SRC_DIR)/json_config_cli.o: $(SRC_DIR)/json_config_cli.c $(SRC_DIR)/json_config.h
