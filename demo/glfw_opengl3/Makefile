# Install
BIN = demo

# Flags
CFLAGS += -g -std=c99 -Wall -Wextra -pedantic -I../..

SRC = main.c
OBJ = $(SRC:.c=.o)
DOCK_SRC = dock_demo.c ../common/nk_dock.c
DOCK_OBJ = $(DOCK_SRC:.c=.o)

# Use a local GLEW shim when the system lacks libglew-dev.
ifeq ($(wildcard /usr/include/GL/glew.h),)
	CFLAGS += -Iglew_shim
	SHIM_GL := -lGL
else
	SHIM_GL := -lGLEW
endif

ifeq ($(OS),Windows_NT)
BIN := $(BIN).exe
LIBS = -lglfw3 -lopengl32 -lm -lGLU32 -lGLEW32
else
	UNAME_S := $(shell uname -s)
	GLFW3 := $(shell pkg-config --libs glfw3)
	ifeq ($(UNAME_S),Darwin)
		LIBS := $(GLFW3) -framework OpenGL -framework Cocoa -framework IOKit -framework CoreVideo -lm -lGLEW -L/usr/local/lib
else
	LIBS = $(GLFW3) -lGL -lm $(SHIM_GL)
endif
endif

$(BIN):
	@mkdir -p bin
	rm -f bin/$(BIN) $(OBJS)
	$(CC) $(SRC) $(CFLAGS) -o bin/$(BIN) $(LIBS)

dock_demo:
	@mkdir -p bin
	rm -f bin/dock_demo $(DOCK_OBJ)
	$(CC) $(DOCK_SRC) $(CFLAGS) -o bin/dock_demo $(LIBS)

dock_test:
	@mkdir -p bin
	rm -f bin/dock_test
	$(CC) dock_test.c ../common/nk_dock.c $(CFLAGS) -o bin/dock_test -lm
