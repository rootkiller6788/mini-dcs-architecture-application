# Makefile for mini-supcon-ecs700-dcs
# SUPCON ECS-700 Distributed Control System
#
# Compiler: gcc (C11)
# Lean 4:   lake (Lean 4 proof assistant)
#
# Targets:
#   make all      — build everything
#   make test     — build and run tests
#   make examples — build examples
#   make benches  — build benchmarks
#   make demos    — build demos
#   make clean    — remove build artifacts
#   make lines    — count lines in include/ + src/

CC       := gcc
CFLAGS   := -Wall -Wextra -std=c11 -O2 -g
LDFLAGS  := -lm
INCLUDES := -Iinclude

BUILD_DIR := build

SRC_FILES := src/ecs700_system_core.c src/ecs700_control_station.c \
             src/ecs700_redundancy.c src/ecs700_communication.c \
             src/ecs700_io_subsystem.c

TEST_FILES := tests/test_system_core.c tests/test_control_station.c \
              tests/test_redundancy.c tests/test_communication.c \
              tests/test_io_subsystem.c

EXAMPLE_FILES := examples/example_chemical_reactor.c \
                 examples/example_distillation_column.c \
                 examples/example_boiler_control.c

BENCH_FILES := benches/bench_redundancy.c

DEMO_FILES := demos/demo_dcs_simulation.c

SRC_OBJS  := $(patsubst src/%.c,$(BUILD_DIR)/%.o,$(SRC_FILES))
TEST_BINS := $(patsubst tests/%.c,$(BUILD_DIR)/%,$(TEST_FILES))
EXAMPLE_BINS := $(patsubst examples/%.c,$(BUILD_DIR)/%,$(EXAMPLE_FILES))
BENCH_BINS := $(patsubst benches/%.c,$(BUILD_DIR)/%,$(BENCH_FILES))
DEMO_BINS := $(patsubst demos/%.c,$(BUILD_DIR)/%,$(DEMO_FILES))

.PHONY: all
all: test examples benches demos

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/%.o: src/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# --- Tests ---
$(BUILD_DIR)/test_system_core: tests/test_system_core.c $(SRC_OBJS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) $< $(SRC_OBJS) -o $@ $(LDFLAGS)

$(BUILD_DIR)/test_control_station: tests/test_control_station.c $(SRC_OBJS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) $< $(SRC_OBJS) -o $@ $(LDFLAGS)

$(BUILD_DIR)/test_redundancy: tests/test_redundancy.c $(SRC_OBJS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) $< $(SRC_OBJS) -o $@ $(LDFLAGS)

$(BUILD_DIR)/test_communication: tests/test_communication.c $(SRC_OBJS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) $< $(SRC_OBJS) -o $@ $(LDFLAGS)

$(BUILD_DIR)/test_io_subsystem: tests/test_io_subsystem.c $(SRC_OBJS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) $< $(SRC_OBJS) -o $@ $(LDFLAGS)

.PHONY: test
test: $(TEST_BINS)
	@echo "=== Running All Tests ==="
	-$(BUILD_DIR)/test_system_core
	-$(BUILD_DIR)/test_control_station
	-$(BUILD_DIR)/test_redundancy
	-$(BUILD_DIR)/test_communication
	-$(BUILD_DIR)/test_io_subsystem
	@echo "=== All Tests Done ==="

# --- Examples ---
$(BUILD_DIR)/example_chemical_reactor: examples/example_chemical_reactor.c $(SRC_OBJS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) $< $(SRC_OBJS) -o $@ $(LDFLAGS)

$(BUILD_DIR)/example_distillation_column: examples/example_distillation_column.c $(SRC_OBJS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) $< $(SRC_OBJS) -o $@ $(LDFLAGS)

$(BUILD_DIR)/example_boiler_control: examples/example_boiler_control.c $(SRC_OBJS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) $< $(SRC_OBJS) -o $@ $(LDFLAGS)

.PHONY: examples
examples: $(EXAMPLE_BINS)
	@echo "=== All examples built ==="

# --- Benches ---
$(BUILD_DIR)/bench_redundancy: benches/bench_redundancy.c $(SRC_OBJS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) $< $(SRC_OBJS) -o $@ $(LDFLAGS)

.PHONY: benches
benches: $(BENCH_BINS)
	@echo "=== All benches built ==="

# --- Demos ---
$(BUILD_DIR)/demo_dcs_simulation: demos/demo_dcs_simulation.c $(SRC_OBJS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) $< $(SRC_OBJS) -o $@ $(LDFLAGS)

.PHONY: demos
demos: $(DEMO_BINS)
	@echo "=== All demos built ==="

.PHONY: clean
clean:
	rm -rf $(BUILD_DIR)

.PHONY: lines
lines:
	@echo "include/ lines:" && find include -name "*.h" -exec cat {} + | wc -l
	@echo "src/ lines:" && find src -name "*.c" -o -name "*.lean" | xargs cat | wc -l
	@echo "total lines:" && find include src -type f \( -name "*.h" -o -name "*.c" -o -name "*.lean" \) -exec cat {} + | wc -l
