#include <cstdlib>
#include <cstdio>
#include <string>
#include <atomic>
#include <cassert>

#define MEMSIZE             4096
#define STACKSIZE           32
#define NREGISTERS          16
#define ROM_START_ADDR      0x200
#define DISPLAY_WIDTH       64
#define DISPLAY_HEIGHT      32
#define SIZEOF(arr)         sizeof(arr) / sizeof(arr[0]);

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
    uint16_t membuf[MEMSIZE] = {0};
    uint16_t stack[STACKSIZE] = {0xFFFF};
    std::atomic<unsigned long> delay_timer;
    std::atomic<unsigned long> sound_timer;
    uint8_t regs [NREGISTERS] = {
        0, 0, 0, 0, // V0-V3
        0, 0, 0, 0, // V4-V7
        0, 0, 0, 0, // V8-VB
        0, 0, 0, 0, // VC-VF 
    };
    uint16_t reg_i;
    char regs_valkeys [NREGISTERS] = {
        // corresponding values are the indices
        'x', '1', '2', '3',
        'q', 'w', 'e', 'a',
        's', 'd', 'z', 'c',
        '4', 'r', 'f', 'v'
    };
    uint16_t PC;
};

//  Combine instructions at PC, PC+1 into one 16-bit
//      instruction. Then prepare to fetch next opcode.
int fetch (Emulator &e, uint16_t PC);

//  Executes 16-bit instruction returned by fetch (e, PC);
int exec (Emulator &e, uint16_t instr);

//  Finds stack space by iterating through stack addresses.
int findstackspace (Emulator &e);

//  Finds address of a given hexadecimal character stored
//      in a particular register (regval).
uint16_t findfontaddress (uint16_t regval);

//  Parses 8NNN instructions.
int parse_8NNN (Emulator &e, uint16_t instr);

//  Parses FNNN instructions.
int parse_FNNN (Emulator &e, uint16_t instr);

//  Scans for character inputs.
char check_input() {
    char key;
    int r = scanf("%s", &key);
    assert(r >= 0);
    return key;
}

//  Checks if character input corresponds to a hexadecimal value.
int check_keyboard (char &key) {
    return
        (
        key == '1' || key == '2' || key == '3' || key == '4' ||
        key == 'q' || key == 'w' || key == 'e' || key == 'r' ||
        key == 'a' || key == 's' || key == 'd' || key == 'f' ||
        key == 'z' || key == 'x' || key == 'c' || key == 'v'
        );
}
