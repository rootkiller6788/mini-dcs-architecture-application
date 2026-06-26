# Makefile ? mini-fieldbus-foundation-h1
# Foundation Fieldbus H1 Protocol Stack

CC       = gcc
CFLAGS   = -std=c11 -Wall -Wextra -O2 -Iinclude
LDFLAGS  = -lm
LEAN     = lean

SRC = src/ff_h1_physical.c src/ff_h1_datalink.c src/ff_h1_application.c src/ff_h1_system_mgmt.c src/ff_h1_device.c src/ff_h1_segment.c
OBJ = $(SRC:.c=.o)
LIB = libffh1.a

TEST_SRC = tests/test_physical.c tests/test_datalink.c tests/test_application.c tests/test_system_mgmt.c tests/test_segment.c
TEST_BIN = $(TEST_SRC:.c=.exe)

EXAMPLE_SRC = examples/example_segment_design.c examples/example_las_schedule.c examples/example_device_commission.c
EXAMPLE_BIN = $(EXAMPLE_SRC:.c=.exe)

DEMO_SRC = demos/demo_segment_sim.c
DEMO_BIN = $(DEMO_SRC:.c=.exe)

BENCH_SRC = benches/bench_crc16.c
BENCH_BIN = $(BENCH_SRC:.c=.exe)

.PHONY: all lib test examples demo bench clean lean-check

all: lib test examples

lib: $(LIB)

$(LIB): $(OBJ)
	ar rcs $@ $^

src/%.o: src/%.c include/*.h
	$(CC) $(CFLAGS) -c $< -o $@

tests/test_%.exe: tests/test_%.c $(LIB)
	$(CC) $(CFLAGS) $< -L. -lffh1 $(LDFLAGS) -o $@

examples/%.exe: examples/%.c $(LIB)
	$(CC) $(CFLAGS) $< -L. -lffh1 $(LDFLAGS) -o $@

demos/%.exe: demos/%.c $(LIB)
	$(CC) $(CFLAGS) $< -L. -lffh1 $(LDFLAGS) -o $@

benches/%.exe: benches/%.c $(LIB)
	$(CC) $(CFLAGS) $< -L. -lffh1 $(LDFLAGS) -o $@

test: $(TEST_BIN)
	@echo "=== Running tests ==="
	@for t in $(TEST_BIN); do echo "--- $$t ---"; ./$$t || exit 1; done
	@echo "=== All tests passed ==="

examples: $(EXAMPLE_BIN)
	@echo "=== Running examples ==="
	@for e in $(EXAMPLE_BIN); do echo "--- $$e ---"; ./$$e || exit 1; done
	@echo "=== All examples OK ==="

demo: $(DEMO_BIN)
	./$(DEMO_BIN)

bench: $(BENCH_BIN)
	./$(BENCH_BIN)

lean-check:
	@echo "=== Lean 4 formalization check ==="
	$(LEAN) src/ff_h1_formal.lean
	@echo "=== Lean check passed ==="

clean:
	rm -f src/*.o $(LIB) tests/*.exe examples/*.exe demos/*.exe benches/*.exe

debug: CFLAGS += -g -DDEBUG
debug: all
