CXX := g++
CXXFLAGS := -Wall -Wextra -Werror -std=c++17 -O2 -I./src

SRC_DIR := src
OBJ_DIR := build

AGENT_SRC := $(SRC_DIR)/agents.cpp
MANAGER_SRC := $(SRC_DIR)/manager.cpp

AGENT_OBJ := $(OBJ_DIR)/agents.o
MANAGER_OBJ := $(OBJ_DIR)/manager.o

AGENT_BIN := agent
MANAGER_BIN := manager

.PHONY: all clean

all: $(AGENT_BIN) $(MANAGER_BIN)

$(AGENT_BIN): $(AGENT_OBJ)
	$(CXX) $(CXXFLAGS) $^ -o $@

$(MANAGER_BIN): $(MANAGER_OBJ)
	$(CXX) $(CXXFLAGS) $^ -o $@

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

$(AGENT_OBJ): $(AGENT_SRC) | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(MANAGER_OBJ): $(MANAGER_SRC) | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -rf $(OBJ_DIR) $(AGENT_BIN) $(MANAGER_BIN)
