emu: emulator.o
	@g++ emulator.o -o emu

emulator.o: emulator.cc
	@g++ -c emulator.cc

clean:
	$(info CLEAN) @rm *.o emu