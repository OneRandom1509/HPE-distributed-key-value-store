PROJECT_NAME := kvserver
BUILD_DIR := build
BUILD_DBG_DIR := build-dbg

CMAKE := cmake

# ------------------------------------------------
# Release build
# ------------------------------------------------

build:
	@echo "▶ Configuring Release"
	+@$(CMAKE) -S . -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=Release

	@echo "▶ Building"
	+@$(CMAKE) --build $(BUILD_DIR)

all: build

# Clean rebuild
rebuild:
	@echo "▶ Rebuilding (clean + build)"
	@rm -rf $(BUILD_DIR)
	@$(MAKE) build

# ------------------------------------------------
# Debug build
# ------------------------------------------------

debug:
	@echo "▶ Configuring Debug"
	+@$(CMAKE) -S . -B $(BUILD_DBG_DIR) -DCMAKE_BUILD_TYPE=Debug

	@echo "▶ Building Debug"
	+@$(CMAKE) --build $(BUILD_DBG_DIR)

# ------------------------------------------------
# Run targets
# ------------------------------------------------

server: build
	@./$(BUILD_DIR)/kvm_server $(ARGS)

client: build
	@./$(BUILD_DIR)/kvm_client

nodes: build
	@./$(BUILD_DIR)/start_nodes.sh

test: build
	@ctest --test-dir $(BUILD_DIR) --output-on-failure -V

# Run only the KVDistributor unit test binary (convenience)
test-kv: build
	@if [ -x "$(BUILD_DIR)/kv_distributor_tests" ]; then \
		echo "▶ Running kv_distributor_tests"; \
		"$(BUILD_DIR)/kv_distributor_tests"; \
	else \
		echo "kv_distributor_tests not found. Run 'make build' first."; exit 1; \
	fi

# Run only consistent_hash_ring_tests binary
test-ring: build
	@if [ -x "$(BUILD_DIR)/consistent_hash_ring_tests" ]; then \
		echo "▶ Running consistent_hash_ring_tests"; \
		"$(BUILD_DIR)/consistent_hash_ring_tests"; \
	else \
		echo "consistent_hash_ring_tests not found. Run 'make build' first."; exit 1; \
	fi

# Verbose ctest
test-verbose: build
	@ctest --test-dir $(BUILD_DIR) --output-on-failure -V

# ------------------------------------------------
# Cleaning
# ------------------------------------------------

clean:
	rm -rf $(BUILD_DIR) $(BUILD_DBG_DIR)

# ------------------------------------------------
# Utilities
# ------------------------------------------------

compile_commands: build
	@ln -sf $(BUILD_DIR)/compile_commands.json .

.PHONY: build debug clean server client nodes test
