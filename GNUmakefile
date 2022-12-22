emu: emulator.o
	@g++ emulator.o -c -lSDL2 -o emu

emulator.o: emulator.cc
	@g++ -I/opt/homebrew/Cellar/sdl2/2.24.1/include -c -std=c++17 emulator.cc -o emulator.o

clean:
	$(info CLEAN) @rm *.o emu