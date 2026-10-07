CC := gcc
CFLAGS := -std=c11 -Wall -Wextra -Wpedantic -Werror -O2
THREAD_FLAGS := -pthread
COMMON_SOURCES := common/log.c common/parse.c common/protocol.c
CLIENT_SOURCES := client/client.c client/connection.c client/input.c $(COMMON_SOURCES)
SERVER_SOURCES := server/server.c server/control.c server/queue.c server/stats.c server/worker.c $(COMMON_SOURCES)

.PHONY: all clean

all: client_app server_app

client_app: $(CLIENT_SOURCES)
	$(CC) $(CFLAGS) $(THREAD_FLAGS) $(CLIENT_SOURCES) -o $@

server_app: $(SERVER_SOURCES)
	$(CC) $(CFLAGS) $(THREAD_FLAGS) $(SERVER_SOURCES) -o $@

clean:
	rm -f client_app server_app
