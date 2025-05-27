# Main Makefile for P2P File Sharing System
.PHONY: all clean tracker client run-tracker run-client1 run-client2 setup help

# Compiler settings
CXX = g++
CXXFLAGS = -std=c++11 -Wall -Wextra -pthread -O2

# Default target
all: setup tracker client
	@echo "🎉 Build complete! All components ready."
	@echo "📋 Use 'make help' to see available commands"

# Create necessary files
setup:
	@echo "🔧 Setting up configuration..."
	@echo "127.0.0.1:8080" > tracker_info.txt
	@echo "127.0.0.1:8081" >> tracker_info.txt
	@echo "📁 Created tracker_info.txt"
	@touch test_file.txt
	@echo "Hello P2P World! This is a test file for sharing." > test_file.txt
	@echo "📄 Created test_file.txt for testing"

# Build tracker
tracker:
	@echo "🏗️  Building tracker..."
	$(MAKE) -C tracker
	@echo "✅ Tracker built successfully"

# Build client  
client:
	@echo "🏗️  Building client..."
	$(MAKE) -C client
	@echo "✅ Client built successfully"

# Run tracker (Terminal 1)
run-tracker:
	@echo "🚀 Starting Tracker..."
	@echo "📍 Running on port 8080"
	@echo "⚠️  Keep this terminal open"
	./tracker/tracker tracker_info.txt 0

# Run first client (Terminal 2)
run-client1:
	@echo "👤 Starting Client 1..."
	@echo "📍 Client address: 127.0.0.1:9001"
	@echo "💡 Tip: Create user 'alice' and group 'testgroup'"
	./client/client 127.0.0.1:9001 tracker_info.txt

# Run second client (Terminal 3)  
run-client2:
	@echo "👤 Starting Client 2..."
	@echo "📍 Client address: 127.0.0.1:9002"
	@echo "💡 Tip: Create user 'bob' and join 'testgroup'"
	./client/client 127.0.0.1:9002 tracker_info.txt

# Clean build files
clean:
	@echo "🧹 Cleaning build files..."
	$(MAKE) -C tracker clean
	$(MAKE) -C client clean
	rm -f tracker_info.txt test_file.txt
	@echo "✨ Clean complete"

# Install to system (optional)
install: all
	@echo "📦 Installing P2P File Sharing System..."
	@mkdir -p bin
	@cp tracker/tracker bin/
	@cp client/client bin/
	@cp tracker_info.txt bin/
	@echo "🎯 Installation complete! Binaries in bin/"

# Help menu
help:
	@echo "🚀 P2P File Sharing System - Build Commands"
	@echo "============================================="
	@echo ""
	@echo "📋 BUILD COMMANDS:"
	@echo "  make all        - Build everything (default)"
	@echo "  make tracker    - Build tracker only"
	@echo "  make client     - Build client only"
	@echo "  make setup      - Create config files"
	@echo "  make clean      - Clean all build files"
	@echo "  make install    - Install to bin/ directory"
	@echo ""
	@echo "🏃‍♂️ RUN COMMANDS (use in separate terminals):"
	@echo "  make run-tracker  - Start tracker (Terminal 1)"
	@echo "  make run-client1  - Start client 1 (Terminal 2)"  
	@echo "  make run-client2  - Start client 2 (Terminal 3)"
	@echo ""
	@echo "🔧 QUICK START:"
	@echo "  Terminal 1: make run-tracker"
	@echo "  Terminal 2: make run-client1"
	@echo "  Terminal 3: make run-client2"
	@echo ""
	@echo "💡 TEST WORKFLOW:"
	@echo "  1. Client1: Create user 'alice', login, create group 'testgroup'"
	@echo "  2. Client1: Upload 'test_file.txt' to 'testgroup'"
	@echo "  3. Client2: Create user 'bob', login, join 'testgroup'"
	@echo "  4. Client1: Accept bob's request"
	@echo "  5. Client2: Download 'test_file.txt'"