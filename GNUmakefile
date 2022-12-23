OBJS = emulator.cc

CC = g++

# -w suppresses all warnings
COMPILER_FLAGS = -w $(brew --prefix)/include

LINKER_FLAGS = -I/opt/homebrew/include -L/opt/homebrew/lib/ -lSDL2

OBJ_NAME = emulator

emu : $(OBJS)
	$(CC) $(OBJS) $(COMPILER_FLAGS) $(LINKER_FLAGS) -o $(OBJ_NAME) -v

clean:
	$(info CLEAN) @rm *.o emu