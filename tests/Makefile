 # Compiler and flags
CC := clang
CFLAGS := -v -L../build/output -lswift_net 

# Directories
SRC_DIR := src
OUT_DIR := output

# Source files and targets
SRCS := $(wildcard $(SRC_DIR)/*.c)
BINS := $(patsubst $(SRC_DIR)/%.c,$(OUT_DIR)/%,$(SRCS))

# Default target
all: $(BINS) run_all_tests

run_all_tests:
	$(CC) run_all_tests.c -o run_all_tests

# Rule to build each binary
$(OUT_DIR)/%: $(SRC_DIR)/%.c | $(OUT_DIR)
	$(CC) $(CFLAGS) -o $@ $<

# Create output directory if needed
$(OUT_DIR):
	mkdir -p $(OUT_DIR)

# Clean rule
clean:
	rm -rf $(OUT_DIR)
