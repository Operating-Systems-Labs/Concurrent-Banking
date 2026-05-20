CC = gcc
INCLUDES = -Iinclude
SOURCES = $(wildcard src/*.c)
TARGET = bin/bankdb

CFLAGS_COMMON = -Wall -Wextra -std=c11 -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=500
CFLAGS_RELEASE = $(CFLAGS_COMMON) -O2
CFLAGS_DEBUG = $(CFLAGS_COMMON) -g -O1 -fsanitize=thread

LDFLAGS = -pthread
LDFLAGS_DEBUG = -pthread -fsanitize=thread

.PHONY: all debug debug-all clean test

all: $(TARGET)

$(TARGET): $(SOURCES)
	@mkdir -p bin
	$(CC) $(CFLAGS_RELEASE) $(INCLUDES) $(SOURCES) -o $(TARGET) $(LDFLAGS)

debug:
	@mkdir -p bin
	$(CC) $(CFLAGS_DEBUG) $(INCLUDES) $(SOURCES) -o $(TARGET) $(LDFLAGS_DEBUG)

debug-all: debug
	setarch $(uname -m) -R ./bin/bankdb --accounts=tests/accounts.txt --trace=tests/trace_simple.txt --deadlock=prevention --tick-ms=100
	setarch $(uname -m) -R ./bin/bankdb --accounts=tests/accounts.txt --trace=tests/trace_readers.txt --deadlock=prevention --tick-ms=100
	setarch $(uname -m) -R ./bin/bankdb --accounts=tests/accounts.txt --trace=tests/trace_abort.txt --deadlock=prevention --tick-ms=100
	setarch $(uname -m) -R ./bin/bankdb --accounts=tests/accounts.txt --trace=tests/trace_deadlock.txt --deadlock=prevention --tick-ms=100
	setarch $(uname -m) -R ./bin/bankdb --accounts=tests/accounts.txt --trace=tests/trace_buffer.txt --deadlock=prevention --tick-ms=100
	setarch $(uname -m) -R ./bin/bankdb --accounts=tests/accounts.txt --trace=tests/trace_abort_mid.txt --deadlock=prevention --tick-ms=100
	setarch $(uname -m) -R ./bin/bankdb --accounts=tests/accounts.txt --trace=tests/trace_rw_mix.txt --deadlock=prevention --tick-ms=100

test: all
	./bin/bankdb --accounts=tests/accounts.txt --trace=tests/trace_simple.txt --deadlock=prevention --tick-ms=100
	./bin/bankdb --accounts=tests/accounts.txt --trace=tests/trace_readers.txt --deadlock=prevention --tick-ms=100
	./bin/bankdb --accounts=tests/accounts.txt --trace=tests/trace_abort.txt --deadlock=prevention --tick-ms=100
	./bin/bankdb --accounts=tests/accounts.txt --trace=tests/trace_deadlock.txt --deadlock=prevention --tick-ms=100
	./bin/bankdb --accounts=tests/accounts.txt --trace=tests/trace_buffer.txt --deadlock=prevention --tick-ms=100

clean:
	rm -rf bin/bankdb
