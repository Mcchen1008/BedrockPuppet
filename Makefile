# BedrockPuppet Makefile
# C++17 | clang++ | Linux epoll

CXX        := clang++
CXXFLAGS   := -std=c++17 -Wall -Wextra -O2 -pthread
LDFLAGS    := -lssl -lcrypto -pthread

# Directories
SRC_DIR    := src
BUILD_DIR  := build
BIN_DIR    := bin

# Source discovery
COMMON_SRC := $(wildcard $(SRC_DIR)/common/*.cpp)
WS_SRC     := $(wildcard $(SRC_DIR)/ws/*.cpp)
HTTP_SRC   := $(wildcard $(SRC_DIR)/http_api/*.cpp)
LOGIC_SRC  := $(wildcard $(SRC_DIR)/logic/*.cpp)
DAEMON_SRC := $(wildcard $(SRC_DIR)/daemon/*.cpp)

# Object files
COMMON_OBJ := $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(COMMON_SRC))
WS_OBJ     := $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(WS_SRC))
HTTP_OBJ   := $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(HTTP_SRC))
LOGIC_OBJ  := $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(LOGIC_SRC))
DAEMON_OBJ := $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(DAEMON_SRC))

# Targets
TARGETS    := $(BIN_DIR)/bedrock_puppet $(BIN_DIR)/ws $(BIN_DIR)/http_api $(BIN_DIR)/logic

.PHONY: all clean

all: $(TARGETS)
	@echo "✅ Build complete. Binaries in $(BIN_DIR)/"

# --- Link targets ---

$(BIN_DIR)/bedrock_puppet: $(DAEMON_OBJ) $(COMMON_OBJ)
	@mkdir -p $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)
	@echo "  [LINK] $@"

$(BIN_DIR)/ws: $(WS_OBJ) $(COMMON_OBJ)
	@mkdir -p $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)
	@echo "  [LINK] $@"

$(BIN_DIR)/http_api: $(HTTP_OBJ) $(COMMON_OBJ)
	@mkdir -p $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)
	@echo "  [LINK] $@"

$(BIN_DIR)/logic: $(LOGIC_OBJ) $(COMMON_OBJ)
	@mkdir -p $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)
	@echo "  [LINK] $@"

# --- Compile rules ---

$(BUILD_DIR)/common/%.o: $(SRC_DIR)/common/%.cpp
	@mkdir -p $(BUILD_DIR)/common
	$(CXX) $(CXXFLAGS) -I$(SRC_DIR) -c $< -o $@

$(BUILD_DIR)/ws/%.o: $(SRC_DIR)/ws/%.cpp
	@mkdir -p $(BUILD_DIR)/ws
	$(CXX) $(CXXFLAGS) -I$(SRC_DIR) -c $< -o $@

$(BUILD_DIR)/http_api/%.o: $(SRC_DIR)/http_api/%.cpp
	@mkdir -p $(BUILD_DIR)/http_api
	$(CXX) $(CXXFLAGS) -I$(SRC_DIR) -c $< -o $@

$(BUILD_DIR)/logic/%.o: $(SRC_DIR)/logic/%.cpp
	@mkdir -p $(BUILD_DIR)/logic
	$(CXX) $(CXXFLAGS) -I$(SRC_DIR) -c $< -o $@

$(BUILD_DIR)/daemon/%.o: $(SRC_DIR)/daemon/%.cpp
	@mkdir -p $(BUILD_DIR)/daemon
	$(CXX) $(CXXFLAGS) -I$(SRC_DIR) -c $< -o $@

clean:
	rm -rf $(BUILD_DIR) $(BIN_DIR)/bedrock_puppet $(BIN_DIR)/ws $(BIN_DIR)/http_api $(BIN_DIR)/logic
	@echo "🧹 Clean complete."
