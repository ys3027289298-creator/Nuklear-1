# Property table demo
BIN = property_table

CFLAGS += -g -std=c99 -Wall -Wextra -pedantic -DOUTPUT_DIR='"$(OUTPUT_DIR)"'
OUTPUT_DIR ?= output

SRC = property_table.c
OBJ = $(SRC:.c=.o)

ifeq ($(OS),Windows_NT)
BIN := $(BIN).exe
LIBS = -lglfw3 -lopengl32 -lm
else
	UNAME_S := $(shell uname -s)
	GLFW3 := $(shell pkg-config --libs glfw3)
	ifeq ($(UNAME_S),Darwin)
		LIBS := $(GLFW3) -framework OpenGL -framework Cocoa -framework IOKit -framework CoreVideo -lm
	else
		LIBS = $(GLFW3) -lGL -lm
	endif
endif

$(BIN):
	@mkdir -p bin
	rm -f bin/$(BIN) $(OBJS)
	$(CC) $(SRC) $(CFLAGS) -o bin/$(BIN) $(LIBS)

clean:
	rm -rf bin
