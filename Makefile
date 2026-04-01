CC = gcc
CFLAGS = -Wall -Wextra -O2 -std=c99 -I./include -lm
SRC_DIR = src
BIN_DIR = bin
TEST_DIR = tests

SOURCES = $(SRC_DIR)/main.c $(SRC_DIR)/qpsk.c $(SRC_DIR)/wav.c $(SRC_DIR)/reed_solomon.c
HEADERS = $(wildcard include/*.h)
OBJECTS = $(patsubst $(SRC_DIR)/%.c,$(BIN_DIR)/%.o,$(SOURCES))
TARGET = $(BIN_DIR)/encoder

TEST_SOURCES = test_qpsk.c test_reed_solomon.c
TEST_TARGETS = $(patsubst %.c,$(BIN_DIR)/%,$(TEST_SOURCES))
TEST_OBJECTS = $(BIN_DIR)/qpsk.o $(BIN_DIR)/wav.o $(BIN_DIR)/reed_solomon.o

.PHONY: all clean test tests help

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) -o $@ -lm
	@echo "Build complete: $(TARGET)"

$(BIN_DIR)/%.o: $(SRC_DIR)/%.c $(HEADERS)
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

test: $(TARGET)
	@echo "Running basic tests..."
	@echo "Test 1: Encoding test data..."
	@echo "Hello, World! This is a test message for QPSK encoding with Reed-Solomon error correction." > /tmp/test_input.txt
	$(TARGET) -e /tmp/test_input.txt /tmp/test_output.wav
	@echo "Test 2: Decoding audio..."
	$(TARGET) -d /tmp/test_output.wav /tmp/test_decoded.txt
	@echo "Test 3: Verifying decoded output..."
	@if diff /tmp/test_input.txt /tmp/test_decoded.txt > /dev/null 2>&1; then \
		echo "✓ Encode/decode test passed!"; \
	else \
		echo "✗ Encode/decode test failed!"; \
		echo "Original:"; cat /tmp/test_input.txt; \
		echo "\nDecoded:"; cat /tmp/test_decoded.txt; \
	fi
	@rm -f /tmp/test_input.txt /tmp/test_output.wav /tmp/test_decoded.txt

$(BIN_DIR)/test_%: $(TEST_DIR)/test_%.c $(TEST_OBJECTS) $(HEADERS)
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) -c $(TEST_DIR)/test_$*.c -o $(BIN_DIR)/test_$*.o
	$(CC) $(BIN_DIR)/test_$*.o $(TEST_OBJECTS) -o $@ -lm

tests: $(TEST_TARGETS)
	@echo "\n╔════════════════════════════════════╗"
	@echo "║    Running Unit Tests              ║"
	@echo "╚════════════════════════════════════╝"
	@echo "\n[1/2] Running QPSK tests..."
	@$(BIN_DIR)/test_qpsk
	@echo "\n[2/2] Running Reed-Solomon tests..."
	@$(BIN_DIR)/test_reed_solomon
	@echo "\n✅ All unit test suites completed!"

clean:
	rm -rf $(BIN_DIR)
	@echo "Clean complete"

help:
	@echo "Audio Encoder - Makefile targets:"
	@echo "  all      - Build the encoder executable"
	@echo "  test     - Run integration tests (encode/decode round-trip)"
	@echo "  tests    - Run all unit tests (QPSK and Reed-Solomon)"
	@echo "  clean    - Remove build artifacts"
	@echo "  help     - Show this help message"
	@echo ""
	@echo "Unit Test Targets:"
	@echo "  $(BIN_DIR)/test_qpsk            - QPSK encoder/decoder tests"
	@echo "  $(BIN_DIR)/test_reed_solomon    - Reed-Solomon error correction tests"
