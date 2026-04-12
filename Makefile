CC = gcc
CFLAGS = -O2 -Wall -Wextra -pedantic
TARGET = dispatcher
SRC = src/dispatcher.c

INPUT_DIR = ./input
WORKER = ./scripts/worker.sh
FINALIZER = ./scripts/finalize.sh
PARALLELISM = 2
OUTPUT_DIR = ./work
RESULT_FILE = ./book.djvu
EXT = .tiff

.PHONY: all run clean prepare

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET)

prepare:
	chmod +x $(WORKER) $(FINALIZER)

run: all prepare
	./$(TARGET) $(INPUT_DIR) $(WORKER) $(FINALIZER) $(PARALLELISM) $(OUTPUT_DIR) $(RESULT_FILE) $(EXT)

clean:
	rm -f $(TARGET) $(RESULT_FILE)
