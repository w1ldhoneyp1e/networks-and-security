CXX := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -Wpedantic -Werror -O2

TARGET := webserver
SOURCES := client/main.cpp server/web_server.cpp server/static_files.cpp http/request.cpp http/response.cpp common/io.cpp

.PHONY: all test clean

all: $(TARGET)

$(TARGET): $(SOURCES)
	$(CXX) $(CXXFLAGS) $(SOURCES) -o $@

test: $(TARGET)
	python3 tests/run_tests.py ./$(TARGET)

clean:
	rm -f $(TARGET)
