# Client Makefile  
CXX = g++
CXXFLAGS = -std=c++11 -Wall -Wextra -pthread -O2
TARGET = client
SOURCES = client.cpp
HEADERS = client.h sha1.h

$(TARGET): $(SOURCES) $(HEADERS)
	@echo "🔨 Compiling $(TARGET)..."
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SOURCES)

clean:
	@echo "🧹 Cleaning client..."
	rm -f $(TARGET)

.PHONY: clean