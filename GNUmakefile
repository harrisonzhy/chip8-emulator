emu: emulator.o
	@g++ emulator.o -o emu

emulator.o: emulator.cc
	@g++ -c -std=c++17 emulator.cc -o emulator.o

clean:
	$(info CLEAN) @rm *.o emu