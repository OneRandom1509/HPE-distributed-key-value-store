PROJECT_NAME := kvserver
BUILD_DIR := build
BUILD_DBG_DIR := build-dbg

CMAKE := cmake

# ------------------------------------------------
# Release build
# ------------------------------------------------

build:
	@echo "▶ Configuring Release"
	@$(CMAKE) -S . -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=Release

	@echo "▶ Building"
	@$(CMAKE) --build $(BUILD_DIR)

# ------------------------------------------------
# Debug build
# ------------------------------------------------

debug:
	@echo "▶ Configuring Debug"
	@$(CMAKE) -S . -B $(BUILD_DBG_DIR) -DCMAKE_BUILD_TYPE=Debug

	@echo "▶ Building Debug"
	@$(CMAKE) --build $(BUILD_DBG_DIR)

# ------------------------------------------------
# Run targets
# ------------------------------------------------

server: build
	@./$(BUILD_DIR)/kvm_server

client: build
	@./$(BUILD_DIR)/kvm_client

nodes: build
	@./$(BUILD_DIR)/start_nodes.sh

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

.PHONY: build debug clean server client nodes
