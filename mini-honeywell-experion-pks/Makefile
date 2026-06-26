# Honeywell Experion PKS - Makefile
# mini-honeywell-experion-pks submodule

CC       = gcc
CFLAGS   = -std=c11 -Wall -Wextra -O2 -g
LDFLAGS  = -lm -Wl,--stack,16777216
INCLUDE  = -Iinclude

SRC_DIR  = src
BUILD_DIR = build
TEST_DIR = tests
EXAMPLES_DIR = examples

SRCS     = src/c300_controller.c src/cee_execution.c src/control_blocks.c src/dcs_redundancy.c src/experion_cab_bulk.c src/experion_system.c src/hmiweb_display.c
OBJS     = $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SRCS))

TEST_SRCS = $(wildcard $(TEST_DIR)/*.c)
TEST_BINS = $(patsubst $(TEST_DIR)/%.c,$(BUILD_DIR)/test_%,$(TEST_SRCS))

EXAMPLE_SRCS = $(wildcard $(EXAMPLES_DIR)/*.c)
EXAMPLE_BINS = $(patsubst $(EXAMPLES_DIR)/%.c,$(BUILD_DIR)/example_%,$(EXAMPLE_SRCS))

.PHONY: all test examples clean lib check

all: lib

lib: $(OBJS)
	@echo "  AR    libexperion_pks.a"
	@ar rcs $(BUILD_DIR)/libexperion_pks.a $(OBJS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(BUILD_DIR)
	@echo "  CC    $<"
	@$(CC) $(CFLAGS) $(INCLUDE) -c $< -o $@

test: $(OBJS) $(TEST_BINS)
	@echo "=== Running Tests ==="
	@for t in $(TEST_BINS); do echo "--- $$t ---"; $$t || exit 1; done
	@echo "=== All Tests Passed ==="

$(BUILD_DIR)/test_%: $(TEST_DIR)/%.c $(OBJS)
	@mkdir -p $(BUILD_DIR)
	@$(CC) $(CFLAGS) $(INCLUDE) $< $(OBJS) $(LDFLAGS) -o $@

examples: $(EXAMPLE_BINS)
	@echo "=== Running Examples ==="
	@for e in $(EXAMPLE_BINS); do echo "--- $$e ---"; $$e; done

$(BUILD_DIR)/example_%: $(EXAMPLES_DIR)/%.c $(OBJS)
	@mkdir -p $(BUILD_DIR)
	@$(CC) $(CFLAGS) $(INCLUDE) $< $(OBJS) $(LDFLAGS) -o $@

clean:
	rm -rf $(BUILD_DIR)

check:
	@for s in $(SRCS); do echo "  CC $$s"; $(CC) $(CFLAGS) -fsyntax-only $(INCLUDE) $$s || exit 1; done
	@echo "=== All Sources Compile ==="

# Individual test run
test_only: $(OBJS) $(TEST_BINS)
	@$(BUILD_DIR)/test_test_core
