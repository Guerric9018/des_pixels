SRC = $(wildcard src/*.cpp)
OBJ = $(SRC:src/%.cpp=bin/%.o)
BIN = main

CFLAGS = -ggdb3 -I include

all:: $(BIN)

clean::
	rm -f $(BIN) $(OBJ)

main: $(OBJ)
	$(CXX) $(LDFLAGS) -o $@ $^

bin/%.o: src/%.cpp
	$(CXX) $(CFLAGS) -c -o $@ $<

bin/%.o: src/%.c

