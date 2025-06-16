SRC = $(wildcard src/*.cpp) src/glad.c
OBJ = $(SRC:src/%=bin/%.o)
BIN = main

LDFLAGS = -lm -lglfw -lGL -lX11 -pthread -lXrandr -lXi -dl
CXXFLAGS = -ggdb3 -I include -std=c++20
CFLAGS   = -ggdb3 -I include

all:: $(BIN)

clean::
	rm -f $(BIN) $(OBJ)

main: $(OBJ)
	$(CXX) -o $@ $^ $(LDFLAGS)

bin/%.cpp.o: src/%.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

bin/%.c.o: src/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

