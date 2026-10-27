CXX := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -Wpedantic -Werror -O2

TARGET := smtp_client
SOURCES := client/main.cpp client/smtp_client.cpp common/socket_connection.cpp common/smtp_reply.cpp message/email.cpp

.PHONY: all test clean

all: $(TARGET)

$(TARGET): $(SOURCES)
	$(CXX) $(CXXFLAGS) $(SOURCES) -o $@

test: $(TARGET)
	python3 tests/run_tests.py ./$(TARGET)

clean:
	rm -f $(TARGET)
