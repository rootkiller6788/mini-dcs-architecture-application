# Makefile for mini-dcs-system-architecture
# DCS System Architecture — ISA-95/IEC 61508/ISA-88

CC       := gcc
CFLAGS   := -Wall -Wextra -std=c11 -O2 -g
LDFLAGS  := -lm
INCLUDES := -Iinclude

BUILD_DIR := build

SRC_FILES := src/dcs_architecture.c src/dcs_redundancy.c src/dcs_alarm.c \
             src/dcs_safety.c src/dcs_system_db.c src/dcs_scan_cycle.c \
             src/dcs_network_analysis.c src/dcs_isa88_batch.c \
             src/dcs_industrial_applications.c

TEST_FILES := tests/test_architecture.c tests/test_redundancy.c \
              tests/test_alarm_safety.c tests/test_system_db.c

EXAMPLE_FILES := examples/example_dcs_system_sizing.c \
                 examples/example_safety_sif_calculation.c \
                 examples/example_batch_reactor_control.c

BENCH_FILES := benches/bench_architecture_perf.c

DEMO_FILES := demos/demo_topology_visual.c

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

# --- Test Targets ---

$(BUILD_DIR)/test_architecture: tests/test_architecture.c $(SRC_OBJS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) $< $(SRC_OBJS) -o $@ $(LDFLAGS)

$(BUILD_DIR)/test_redundancy: tests/test_redundancy.c $(SRC_OBJS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) $< $(SRC_OBJS) -o $@ $(LDFLAGS)

$(BUILD_DIR)/test_alarm_safety: tests/test_alarm_safety.c $(SRC_OBJS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) $< $(SRC_OBJS) -o $@ $(LDFLAGS)

$(BUILD_DIR)/test_system_db: tests/test_system_db.c $(SRC_OBJS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) $< $(SRC_OBJS) -o $@ $(LDFLAGS)

.PHONY: test
test: $(TEST_BINS)
	@echo "=== Running All Tests ==="
	-$(BUILD_DIR)/test_architecture
	-$(BUILD_DIR)/test_redundancy
	-$(BUILD_DIR)/test_alarm_safety
	-$(BUILD_DIR)/test_system_db
	@echo "=== All Tests Done ==="

# --- Example Targets ---

$(BUILD_DIR)/example_dcs_system_sizing: examples/example_dcs_system_sizing.c $(SRC_OBJS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) $< $(SRC_OBJS) -o $@ $(LDFLAGS)

$(BUILD_DIR)/example_safety_sif_calculation: examples/example_safety_sif_calculation.c $(SRC_OBJS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) $< $(SRC_OBJS) -o $@ $(LDFLAGS)

$(BUILD_DIR)/example_batch_reactor_control: examples/example_batch_reactor_control.c $(SRC_OBJS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) $< $(SRC_OBJS) -o $@ $(LDFLAGS)

.PHONY: examples
examples: $(EXAMPLE_BINS)
	@echo "=== All examples built ==="

# --- Bench Targets ---

$(BUILD_DIR)/bench_architecture_perf: benches/bench_architecture_perf.c $(SRC_OBJS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) $< $(SRC_OBJS) -o $@ $(LDFLAGS)

.PHONY: benches
benches: $(BENCH_BINS)
	@echo "=== All benches built ==="

# --- Demo Targets ---

$(BUILD_DIR)/demo_topology_visual: demos/demo_topology_visual.c $(SRC_OBJS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) $< $(SRC_OBJS) -o $@ $(LDFLAGS)

.PHONY: demos
demos: $(DEMO_BINS)
	@echo "=== All demos built ==="

.PHONY: clean
clean:
	rm -rf $(BUILD_DIR)

.PHONY: lines
lines:
	find include src -type f \( -name "*.h" -o -name "*.c" -o -name "*.lean" \) -exec cat {} + | wc -l
