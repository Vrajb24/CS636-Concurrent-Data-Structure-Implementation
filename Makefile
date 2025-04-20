CC = g++
CFLAGS =  -std=c++17 -O3
LDFLAGS = -pthread
LDLIBS = -latomic

P1_DIR = problem1
P2_DIR = problem2
P3_DIR = problem3
BIN_DIR = bin

P1_EXEC = $(P1_DIR)/problem1
P1_TBB_EXEC = $(P1_DIR)/problem1_tbb
P2_EXEC = $(P2_DIR)/problem2
P3_EXEC = $(P3_DIR)/problem3

BIN_FILES = random_keys_insert.bin random_values_insert.bin random_keys_delete.bin random_keys_search.bin
BIN_PATHS = $(addprefix $(BIN_DIR)/, $(BIN_FILES))

THREADS = 4

all: p1 p2 p3

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

p1: $(P1_EXEC)

$(P1_EXEC): $(wildcard $(P1_DIR)/*.cpp) $(wildcard $(P1_DIR)/*.h)
	$(CC) $(CFLAGS) $(P1_DIR)/*.cpp -o $(P1_EXEC) $(LDFLAGS) $(LDLIBS)

p1_tbb: $(P1_TBB_EXEC)

$(P1_TBB_EXEC): $(wildcard $(P1_DIR)/*.cpp) $(wildcard $(P1_DIR)/*.h)
	$(CC) $(CFLAGS) $(P1_DIR)/*.cpp -o $(P1_TBB_EXEC) -DUSE_TBB $(LDFLAGS) $(LDLIBS) -ltbb

$(BIN_DIR)/%.bin: $(P1_DIR)/%.bin | $(BIN_DIR)
	cp $< $@

p2: $(P2_EXEC)

$(P2_EXEC): $(wildcard $(P2_DIR)/*.cpp) $(wildcard $(P2_DIR)/*.h)
	$(CC) $(CFLAGS) $(P2_DIR)/*.cpp -o $(P2_EXEC) $(LDFLAGS) $(LDLIBS)

p3: $(P3_EXEC)

$(P3_EXEC): $(wildcard $(P3_DIR)/*.cpp) $(wildcard $(P3_DIR)/*.h)
	$(CC) $(CFLAGS) $(P3_DIR)/*.cpp -o $(P3_EXEC) $(LDFLAGS) $(LDLIBS)

p1_test: p1 $(BIN_PATHS)
	./$(P1_EXEC) --tests-only --bin-dir $(BIN_DIR)

p1_benchmark: p1 $(BIN_PATHS)
	./$(P1_EXEC) --benchmarks-only --threads $(THREADS) --bin-dir $(BIN_DIR)

p1_compare: p1 p1_tbb $(BIN_PATHS)
	@echo "Running Pthread implementation with $(THREADS) threads..."
	./$(P1_EXEC) --benchmarks-only --threads $(THREADS) --bin-dir $(BIN_DIR)
	@echo "\nRunning TBB implementation with $(THREADS) threads..."
	./$(P1_TBB_EXEC) --benchmarks-only --threads $(THREADS) --bin-dir $(BIN_DIR)

p2_test: p2
	./$(P2_EXEC) --tests-only

p2_benchmark: p2
	./$(P2_EXEC) --benchmarks-only --threads $(THREADS)

p2_compare: p2
	@echo "Running MS Queue implementation with $(THREADS) threads..."
	./$(P2_EXEC) performance --threads $(THREADS) --bin-dir $(BIN_DIR)
	@echo "\nRunning Boost implementation with $(THREADS) threads..."
	./$(P2_EXEC) boost --threads $(THREADS)  --bin-dir $(BIN_DIR)


p3_test: p3
	./$(P3_EXEC) --tests-only

p3_benchmark: p3
	./$(P3_EXEC) --benchmarks-only --threads $(THREADS)

test: p1_test p2_test p3_test

benchmark: p1_benchmark p2_benchmark p3_benchmark

clean:
	rm -f $(P1_EXEC) $(P1_TBB_EXEC) $(P2_EXEC) $(P3_EXEC)

clean_bin:
	rm -f $(BIN_DIR)/*.bin

clean_all: clean clean_bin

.PHONY: all p1 p1_tbb p2 p3 p1_test p1_benchmark p1_compare p2_test p2_benchmark \
        p3_test p3_benchmark test benchmark clean clean_bin clean_all