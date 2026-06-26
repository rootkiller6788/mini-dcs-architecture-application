CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -O2 -g -Iinclude
LDFLAGS = -lm

SRC = src/delta_v_system.c \
      src/delta_v_controller.c \
      src/delta_v_control.c \
      src/delta_v_communication.c \
      src/delta_v_redundancy.c \
      src/delta_v_batch.c

TEST_SRC = tests/test_delta_v.c
TEST_BIN = test_delta_v

EXAMPLE_BINS = example_pid_control_loop example_redundancy_failover example_batch_reactor

.PHONY: all test examples clean run-examples check

all: $(TEST_BIN) examples

$(TEST_BIN): $(SRC) $(TEST_SRC) include/*.h
	$(CC) $(CFLAGS) -o $(TEST_BIN) $(SRC) $(TEST_SRC) $(LDFLAGS)

test: $(TEST_BIN)
	./$(TEST_BIN)

examples: $(EXAMPLE_BINS)

example_pid_control_loop: examples/example_pid_control_loop.c $(SRC) include/*.h
	$(CC) $(CFLAGS) -o $@ examples/example_pid_control_loop.c $(SRC) $(LDFLAGS)

example_redundancy_failover: examples/example_redundancy_failover.c $(SRC) include/*.h
	$(CC) $(CFLAGS) -o $@ examples/example_redundancy_failover.c $(SRC) $(LDFLAGS)

example_batch_reactor: examples/example_batch_reactor.c $(SRC) include/*.h
	$(CC) $(CFLAGS) -o $@ examples/example_batch_reactor.c $(SRC) $(LDFLAGS)

run-examples: examples
	./example_pid_control_loop
	./example_redundancy_failover
	./example_batch_reactor

check:
	@echo "=== Filler Pattern Scan ==="
	@grep -rn "_fn" --include="*.c" --include="*.h" . | grep -E "_fn[0-9]" || true
	@grep -rn "_aux" --include="*.c" --include="*.h" . | grep -E "_aux[0-9]" || true
	@grep -rn "_ext" --include="*.c" --include="*.h" . | grep -E "_ext[0-9]" || true
	@grep -rn "algorithm variant" --include="*.c" . || true
	@grep -rn "auxiliary function" --include="*.c" . || true
	@grep -rn "extension point" --include="*.c" . || true
	@grep -rn "supplemental assert" --include="*.c" . || true
	@grep -rn "by trivial" --include="*.lean" . || true
	@grep -rn "\bsorry\b" --include="*.lean" . || true
	@grep -rn "traceability_matrix.*:=" --include="*.lean" . || true
	@echo "=== Stub Detection ==="
	@find . -name "*.c" -size -200c -exec echo "SUSPECT: {}" \;
	@echo "=== TODO/FIXME/stub/placeholder Scan ==="
	@grep -rn "TODO\|FIXME\|stub\|placeholder" --include="*.c" --include="*.h" --include="*.lean" . || true
	@echo "=== Line Count ==="
	@echo -n "include/ + src/ lines: "
	@find . \( -path "*/include/delta_v_*.h" -o -path "*/src/delta_v_*.c" \) -exec cat {} + | wc -l

clean:
	rm -f $(TEST_BIN) $(EXAMPLE_BINS)
	rm -f src/*.o tests/*.o tests/*.exe
	rm -f include/_test.h include/test_path.h
	rm -f _b.py gen.py gen_all.py
