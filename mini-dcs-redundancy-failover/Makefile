# ============================================================================
# Makefile for mini-dcs-redundancy-failover
# ============================================================================

CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -O2 -g -Iinclude
LDFLAGS = -lm

SRC = src/redundancy_core.c src/failover_engine.c src/voting_mechanism.c src/availability_model.c src/state_sync.c src/diagnostic_monitor.c

TEST_SRC = tests/test_redundancy.c
TEST_BIN = test_redundancy

EXAMPLE_BINS = example_controller_failover example_tmr_voting example_availability_calc

.PHONY: all test examples clean run-examples check

all: $(TEST_BIN) examples

$(TEST_BIN): $(SRC) $(TEST_SRC) include/*.h
	$(CC) $(CFLAGS) -o $(TEST_BIN) $(SRC) $(TEST_SRC) $(LDFLAGS)

test: $(TEST_BIN)
	./$(TEST_BIN)

examples: $(EXAMPLE_BINS)

example_controller_failover: examples/example_controller_failover.c $(SRC) include/*.h
	$(CC) $(CFLAGS) -o $@ examples/example_controller_failover.c $(SRC) $(LDFLAGS)

example_tmr_voting: examples/example_tmr_voting.c $(SRC) include/*.h
	$(CC) $(CFLAGS) -o $@ examples/example_tmr_voting.c $(SRC) $(LDFLAGS)

example_availability_calc: examples/example_availability_calc.c $(SRC) include/*.h
	$(CC) $(CFLAGS) -o $@ examples/example_availability_calc.c $(SRC) $(LDFLAGS)

run-examples: examples
	./example_controller_failover
	./example_tmr_voting
	./example_availability_calc

check:
	@echo "=== Filler Pattern Scan ==="
	@grep -rn "_fn[0-9]" --include="*.c" --include="*.h" . || true
	@grep -rn "_aux[0-9]" --include="*.c" --include="*.h" . || true
	@grep -rn "TODO\|FIXME\|stub\|placeholder" --include="*.c" --include="*.h" . || true
	@echo "=== Line Count ==="
	@find . -path "*/include/*.h" -o -path "*/src/*.c" -exec cat {} + | wc -l

clean:
	rm -f $(TEST_BIN) $(EXAMPLE_BINS) src/*.o tests/*.o
