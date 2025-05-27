# Tracker Makefile
CXX = g++
CXXFLAGS = -std=c++11 -Wall -Wextra -pthread -O2
TARGET = tracker
SOURCES = tracker.cpp
HEADERS = tracker.h

$(TARGET): $(SOURCES) $(HEADERS)
	@echo "🔨 Compiling $(TARGET)..."
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SOURCES)

clean:
	@echo "🧹 Cleaning tracker..."
	rm -f $(TARGET)

.PHONY: clean