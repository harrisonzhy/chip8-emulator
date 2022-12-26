#include <cstdlib>
#include <cstdio>
#include <string>
#include <cassert>
#include <time.h>
#include <fstream>

#define MEMSIZE             4096
#define STACKSIZE           32
#define NREGISTERS          16
#define ROM_START_ADDR      0x200
#define DISPLAY_WIDTH       64
#define DISPLAY_HEIGHT      32
#define TEXEL_SCALE         16
#define TMSLEEP             1850
#define GAME_PATH           "test_opcode.ch8"

struct Emulator {
    uint16_t fontdata[0x09F-0x050+1] =
    {
        0xF0, 0x90, 0x90, 0x90, 0xF0, // 0
        0x20, 0x60, 0x20, 0x20, 0x70, // 1
        0xF0, 0x10, 0xF0, 0x80, 0xF0, // 2
        0xF0, 0x10, 0xF0, 0x10, 0xF0, // 3
        0x90, 0x90, 0xF0, 0x10, 0x10, // 4
        0xF0, 0x80, 0xF0, 0x10, 0xF0, // 5
        0xF0, 0x80, 0xF0, 0x90, 0xF0, // 6
        0xF0, 0x10, 0x20, 0x40, 0x40, // 7
        0xF0, 0x90, 0xF0, 0x90, 0xF0, // 8
        0xF0, 0x90, 0xF0, 0x10, 0xF0, // 9
        0xF0, 0x90, 0xF0, 0x90, 0x90, // A
        0xE0, 0x90, 0xE0, 0x90, 0xE0, // B
        0xF0, 0x80, 0x80, 0x80, 0xF0, // C
        0xE0, 0x90, 0x90, 0x90, 0xE0, // D
        0xF0, 0x80, 0xF0, 0x80, 0xF0, // E
        0xF0, 0x80, 0xF0, 0x80, 0x80  // F
    };
    uint16_t gamedata[MEMSIZE-ROM_START_ADDR] = 
    {

    };
    uint16_t membuf[MEMSIZE] = {0};
    uint16_t stack[STACKSIZE] = {0};
    uint8_t regs[NREGISTERS] = {
        0, 0, 0, 0, // V0-V3
        0, 0, 0, 0, // V4-V7
        0, 0, 0, 0, // V8-VB
        0, 0, 0, 0, // VC-VF 
    };
    uint16_t I;     // index
    char regs_valkeys[NREGISTERS] = {
        'x', '1', '2', '3', // 0-3
        'q', 'w', 'e', 'a', // 4-7
        's', 'd', 'z', 'c', // 7-11
        '4', 'r', 'f', 'v'  // 11-15
    };
    uint16_t PC = ROM_START_ADDR;
    uint8_t delay_timer = UINT8_MAX;
    uint8_t sound_timer = UINT8_MAX;

    void* window;   // window
    void* renderer; // renderer
    char display[DISPLAY_HEIGHT][DISPLAY_WIDTH] = {0};
};

//  Combine instructions at PC, PC+1 into one 16-bit
//      instruction. Then prepare to fetch next opcode.
int fetch (Emulator &e, uint16_t in);

//  Executes 16-bit instruction returned by fetch (e, PC);
int exec (Emulator &e, uint16_t instr);

//  Finds stack space by iterating through stack addresses.
int findstackspace (Emulator &e) {
    for (auto i = STACKSIZE; i != 0; --i) {
        if (e.stack[i-1] == 0xFFFF) {
            return i-1;
        }
    }
    return -1;
}

//  Finds address of a given hexadecimal character stored
//      in a particular register (regval).
uint16_t findfontindex (uint16_t regval) {
    return 0x050 + regval * 5;
}

//  Finds the program counter, i.e. the index of a given
//      address in the memory buffer.
int findpc (Emulator &e, uint16_t addr) {
    for (auto i = 0; i != MEMSIZE; ++i) {
        printf("%d\n", e.membuf[i]);
        if (e.membuf[i] == addr) {
            return i;
        }
    }
    return -1;
}

//  Parses 8NNN instructions.
int parse_8NNN (Emulator &e, uint16_t instr);

//  Parses FNNN instructions.
int parse_FNNN (Emulator &e, uint16_t instr);

//  Scans for character inputs.
char check_input() {    
    char key;
    int r = scanf(" %c", &key);
    assert(r == 0 || r == 1);
    return key;
}

//  Checks if character input corresponds to a hexadecimal value.
int check_keyboard(char &key) {
    return
        (
        key == '1' || key == '2' || key == '3' || key == '4' ||
        key == 'q' || key == 'w' || key == 'e' || key == 'r' ||
        key == 'a' || key == 's' || key == 'd' || key == 'f' ||
        key == 'z' || key == 'x' || key == 'c' || key == 'v'
        );
}

//  Sleeps for `tms` milliseconds.
void msleep(long tms) {
    assert(tms >= 0);
    struct timespec m = {
        0, (tms % 1000) * 1000000
    };
    while (nanosleep(&m, &m));
}

//  Updates delay timer.
void updatedelaytimer (Emulator &e) {
    if (e.delay_timer != 0) {
        --e.delay_timer;
    }
}

//  Updates sleep timer.
void updatesoundtimer (Emulator &e) {
    if (e.sound_timer != 0) {
        --e.sound_timer;
    }
}