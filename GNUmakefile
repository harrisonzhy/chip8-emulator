emu: emulator.o
	@g++ emulator.o -o emu

emulator.o: emulator.cc
	@g++ -std=c++11 emulator.cc

clean:
	$(info CLEAN) @rm *.o emu