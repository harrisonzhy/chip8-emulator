#Executable name
OBJ_NAME = emulator

#Compiler
CC = g++ -std=c++17

LIBRARY_PATHS = -L/opt/homebrew/lib
INCLUDE_PATHS = -I/opt/homebrew/include

OBJS = emulator.cc

COMPILER_FLAGS =

#Libraries
LINKER_FLAGS = -F/Library/Frameworks -lSDL2

emu : $(OBJS)
	$(CC) $(OBJS) $(INCLUDE_PATHS) $(LIBRARY_PATHS) $(COMPILER_FLAGS) $(LINKER_FLAGS) -o emu