CXX := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -Wpedantic -Werror -O2

TARGET := dns_resolver
SOURCES := client/main.cpp resolver/iterative_resolver.cpp common/dns_packet.cpp common/udp_transport.cpp

.PHONY: all test clean

all: $(TARGET)

$(TARGET): $(SOURCES)
	$(CXX) $(CXXFLAGS) $(SOURCES) -o $@

test: $(TARGET)
	python3 tests/run_tests.py ./$(TARGET)

clean:
	rm -f $(TARGET)
