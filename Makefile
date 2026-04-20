CXX := g++
CPPFLAGS := -I.
CXXFLAGS := -std=c++17 -O2 -Wall -Wextra -Wpedantic -pthread
TARGET := concurrent_http_server

SOURCES := \
	main.cpp \
	server/http_server.cpp \
	server/http_parser.cpp \
	threadpool/thread_pool.cpp \
	cache/cache.cpp \
	prefetch/prefetcher.cpp \
	logger/logger.cpp \
	metrics/metrics.cpp \
	utils/file_utils.cpp \
	utils/http_utils.cpp

OBJECTS := $(SOURCES:.cpp=.o)

.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -o $@ $(OBJECTS)

%.o: %.cpp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(OBJECTS) $(TARGET)
