SRC_CPP = $(wildcard src/*.cpp) \
          imgui/imgui.cpp imgui/imgui_draw.cpp imgui/imgui_widgets.cpp imgui/imgui_tables.cpp \
          imgui/imgui_impl_glfw.cpp imgui/imgui_impl_opengl3.cpp
SRC_C = src/glad.c
OBJ_CPP = $(notdir $(SRC_CPP:.cpp=.o))
OBJ_C = $(notdir $(SRC_C:.c=.o))
OBJ = $(addprefix bin/, $(OBJ_CPP) $(OBJ_C))
BIN = main

LDFLAGS = -lm -lglfw -lGL -lX11 -pthread -lXrandr -lXi -dl
CXXFLAGS = -ggdb3 -I include -I imgui -std=c++20
CFLAGS   = -ggdb3 -I include -I imgui

all:: $(BIN)

clean::
	rm -f $(BIN) $(OBJ)

main: $(OBJ)
	$(CXX) -o $@ $^ $(LDFLAGS)

$(BIN): $(OBJ)
	$(CXX) -o $@ $^ $(LDFLAGS)

bin/%.o: src/%.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

bin/%.o: imgui/%.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

bin/%.o: src/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

bin/%.o: imgui/%.c
	$(CC) $(CFLAGS) -c -o $@ $<
