CXX := g++
CXXFLAGS := -Wall -Wextra -Werror -std=c++17 -O2 -I./include

SRC_DIR := src
OBJ_DIR := build

AGENT_OBJ := $(OBJ_DIR)/agents.o $(OBJ_DIR)/server.o
MANAGER_OBJ := $(OBJ_DIR)/manager.o

AGENT_BIN := agent
MANAGER_BIN := manager

.PHONY: all clean

all: $(AGENT_BIN) $(MANAGER_BIN)

$(AGENT_BIN): $(AGENT_OBJ)
	$(CXX) $(CXXFLAGS) $^ -o $@ -pthread

$(MANAGER_BIN): $(MANAGER_OBJ)
	$(CXX) $(CXXFLAGS) $^ -o $@

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -rf $(OBJ_DIR) $(AGENT_BIN) $(MANAGER_BIN)
