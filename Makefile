# Toolchain selection
# Override with: make CROSS_COMPILE=mipsel-linux-gnu-
CROSS_COMPILE ?=

# Default to native compilation
CC = $(CROSS_COMPILE)gcc
AR = $(CROSS_COMPILE)ar
STRIP = $(CROSS_COMPILE)strip

# Flags
CFLAGS_BASE = -Wall -Wextra -std=c99 -pedantic -D_POSIX_C_SOURCE=200809L
CFLAGS = $(CFLAGS_BASE)
CFLAGS_DEBUG = $(CFLAGS_BASE) -g -O0 -DDEBUG
CFLAGS_RELEASE = $(CFLAGS_BASE) -Os -ffunction-sections -fdata-sections
LDFLAGS_BASE =
LDFLAGS = $(LDFLAGS_BASE)
LDFLAGS_RELEASE = $(LDFLAGS_BASE) -Wl,--gc-sections

# Directories and files
SRC_DIR = src
LIB_SOURCES = $(SRC_DIR)/json_value.c $(SRC_DIR)/json_parse.c $(SRC_DIR)/json_serialize.c $(SRC_DIR)/json_config.c $(SRC_DIR)/jsonpath.c
CLI_SOURCES = $(SRC_DIR)/json_config_cli.c
LIB_OBJECTS = $(LIB_SOURCES:.c=.o)
CLI_OBJECTS = $(CLI_SOURCES:.c=.o)
ALL_OBJECTS = $(LIB_OBJECTS) $(CLI_OBJECTS)

# Targets
TARGET_CLI = jct
TARGET_LIB_STATIC = libjct.a
TARGET_LIB_SHARED = libjct.so
SONAME = libjct.so.1
VERSION = 1.0.0

.PHONY: all clean distclean release debug help test lib shared static install

# Default target - build CLI tool
all: $(TARGET_CLI)

# Library targets
lib: static shared
static: $(TARGET_LIB_STATIC)
shared: $(TARGET_LIB_SHARED)

# Help target
help:
	@echo "Available targets:"
	@echo "  make                  - Build CLI tool"
	@echo "  make lib              - Build both static and shared libraries"
	@echo "  make static           - Build static library (libjct.a)"
	@echo "  make shared           - Build shared library (libjct.so)"
	@echo "  make debug            - Build debug version with debug messages"
	@echo "  make release          - Build optimized version"
	@echo "  make install          - Install library and headers"
	@echo "  make clean            - Remove object files and executables"
	@echo "  make distclean        - Remove all generated files"
	@echo "  make test             - Run comprehensive test suite"
	@echo "  make help             - Show this help message"
	@echo ""
	@echo "Using CROSS_COMPILE:"
	@echo "  make CROSS_COMPILE=mipsel-linux-gnu-               - Use toolchain via PATH"
	@echo "  make CROSS_COMPILE=/path/to/toolchain/bin/prefix-  - Use any custom toolchain"

# Debug builds with debug symbols and messages
debug: CFLAGS = $(CFLAGS_DEBUG)
debug: $(TARGET_CLI)

# Release builds with optimization
release: CFLAGS = $(CFLAGS_RELEASE)
release: LDFLAGS = $(LDFLAGS_RELEASE)
release: $(TARGET_CLI)
	$(STRIP) $(TARGET_CLI)

# Build rules
$(TARGET_CLI): $(ALL_OBJECTS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(TARGET_LIB_STATIC): $(LIB_OBJECTS)
	$(AR) rcs $@ $^

$(TARGET_LIB_SHARED): $(LIB_OBJECTS)
	$(CC) -shared -Wl,-soname,$(SONAME) -o $@ $^ $(LDFLAGS)
	ln -sf $(TARGET_LIB_SHARED) $(SONAME)

# Compile library objects with -fPIC for shared library
$(LIB_OBJECTS): $(SRC_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -fPIC -c $< -o $@

# Compile CLI objects normally
$(CLI_OBJECTS): $(SRC_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# Install target
install: $(TARGET_LIB_SHARED) $(TARGET_LIB_STATIC)
	@echo "Installing JCT library..."
	install -d $(DESTDIR)/usr/lib $(DESTDIR)/usr/include
	install -m 644 $(TARGET_LIB_STATIC) $(DESTDIR)/usr/lib/
	install -m 755 $(TARGET_LIB_SHARED) $(DESTDIR)/usr/lib/
	install -m 644 $(SRC_DIR)/json_config.h $(DESTDIR)/usr/include/
	ln -sf $(TARGET_LIB_SHARED) $(DESTDIR)/usr/lib/$(SONAME)

# Test target
test: $(TARGET_CLI)
	@echo "Running comprehensive test suite..."
	@if [ ! -d "test" ]; then \
		echo "Error: test directory not found"; \
		exit 1; \
	fi
	@if [ ! -f "test/run_tests.sh" ]; then \
		echo "Error: test/run_tests.sh not found"; \
		exit 1; \
	fi
	@if [ ! -f "test/test_data.json" ]; then \
		echo "Error: test/test_data.json not found"; \
		exit 1; \
	fi
	@./test/run_tests.sh

clean:
	rm -f $(ALL_OBJECTS) $(TARGET_CLI) $(TARGET_LIB_STATIC) $(TARGET_LIB_SHARED) $(SONAME)
	rm -f json_config_cli json_config_cli.mipsel
	rm -f test/temp_config.json

# Distclean removes all generated files, including object files, executables, and any temporary files
distclean: clean
	rm -f *~ src/*~ *.o src/*.o *.a *.so *.log *.out core core.*

# Dependencies
$(SRC_DIR)/json_value.o: $(SRC_DIR)/json_value.c $(SRC_DIR)/json_config.h
$(SRC_DIR)/json_parse.o: $(SRC_DIR)/json_parse.c $(SRC_DIR)/json_config.h
$(SRC_DIR)/json_serialize.o: $(SRC_DIR)/json_serialize.c $(SRC_DIR)/json_config.h
$(SRC_DIR)/json_config.o: $(SRC_DIR)/json_config.c $(SRC_DIR)/json_config.h

$(SRC_DIR)/jsonpath.o: $(SRC_DIR)/jsonpath.c $(SRC_DIR)/jsonpath.h $(SRC_DIR)/json_config.h

$(SRC_DIR)/json_config_cli.o: $(SRC_DIR)/json_config_cli.c $(SRC_DIR)/json_config.h
