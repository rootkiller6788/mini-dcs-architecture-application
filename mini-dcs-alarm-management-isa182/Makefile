CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -O2 -g -Iinclude
LDFLAGS = -lm

SRC = src/alarm_management_types.c src/alarm_rationalization.c src/alarm_engine.c src/alarm_shelving_suppression.c src/alarm_kpi_metrics.c src/alarm_audit_trail.c

TEST_SRC = tests/test_alarm_management.c
TEST_BIN = test_alarm_management

EXAMPLE_BINS = example_alarm_rationalization example_alarm_engine_live example_alarm_flood_kpi

.PHONY: all test examples clean run-examples

all: $(TEST_BIN) examples

$(TEST_BIN): $(SRC) $(TEST_SRC) include/*.h
	$(CC) $(CFLAGS) -o $(TEST_BIN) $(SRC) $(TEST_SRC) $(LDFLAGS)

test: $(TEST_BIN)
	./$(TEST_BIN)

examples: $(EXAMPLE_BINS)

example_alarm_rationalization: examples/example_alarm_rationalization.c $(SRC) include/*.h
	$(CC) $(CFLAGS) -o example_alarm_rationalization examples/example_alarm_rationalization.c $(SRC) $(LDFLAGS)

example_alarm_engine_live: examples/example_alarm_engine_live.c $(SRC) include/*.h
	$(CC) $(CFLAGS) -o example_alarm_engine_live examples/example_alarm_engine_live.c $(SRC) $(LDFLAGS)

example_alarm_flood_kpi: examples/example_alarm_flood_kpi.c $(SRC) include/*.h
	$(CC) $(CFLAGS) -o example_alarm_flood_kpi examples/example_alarm_flood_kpi.c $(SRC) $(LDFLAGS)

run-examples: examples
	./example_alarm_rationalization
	./example_alarm_engine_live
	./example_alarm_flood_kpi

clean:
	rm -f $(TEST_BIN) $(EXAMPLE_BINS)
	rm -f src/*.o tests/*.o *.exe
