CXX      ?= g++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -pedantic -g

SRC     := $(wildcard src/*.cpp)
HEADERS := $(wildcard src/*.h)

pebble: $(SRC) $(HEADERS)
	$(CXX) $(CXXFLAGS) -o $@ $(SRC)

test: pebble
	./tests/run_tests.sh

clean:
	rm -f pebble

.PHONY: test clean
