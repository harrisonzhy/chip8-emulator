#include "emulator.hh"

#define MEMSIZE             4096
#define STACKSIZE           32
#define NREGISTERS          16
#define ROM_START_ADDR      0x200

#define DISPLAY_WIDTH       64
#define DISPLAY_HEIGHT      32

struct Emulator {
    const uint16_t fontdata[0x09F-0x050+1] = 
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
    std::atomic<unsigned long> delay_timer = 0;
    std::atomic<unsigned long> sound_timer = 0;
    uint8_t regs [NREGISTERS] {
        0, 0, 0, 0, // V0-V3
        0, 0, 0, 0, // V4-V7
        0, 0, 0, 0, // V8-VB
        0, 0, 0, 0  // VC-VF 
    };
    char* PC;
};

int fetch (Emulator &e, char* PC) {
    assert(PC);
    // combine instructions into one 16-bit instruction
    // read instruction that PC is pointing at
    // prepare to fetch next opcode
    PC += 2;
    return 0;
}

int findstackspace (Emulator &e) {
    for (auto i = STACKSIZE; i != 0; --i) {
        if (e.stack[i-1] == 0xFFFF) {
            return i-1;
        }
    }
    return -1;
}

uint16_t parse_8NNN (Emulator &e, uint16_t instr) {
    uint16_t sn  = 0b0000'0000'1111'0000 & instr;    // second nibble to find first reg
    uint16_t tn  = 0b0000'1111'0000'0000 & instr;    // third nibble to find second reg
    uint16_t pn  = 0b1111'0000'0000'0000 & instr;    // fourth nibble
    switch (pn) {
        case 0:
            // 8XY0: set VX to VY   
            sn = tn;
            return sn;
        case 1:
            // 8XY1: set VX to VX | VY
            sn = sn | tn;
            return sn;
        case 2:
            // 8XY2: set VX to VX & VY
            sn = sn & tn;
            return sn;
        case 3:
            // 8XY3: set VX to VX ^ VY
            sn = sn ^ tn;
            return sn;
        case 4:
            // 8XY4: set VX to VX + VY
            uint16_t sn_prev = sn;
            sn += tn;
            // set VF = 1 if 'carry' (overflow) and 0 otherwise
            if (sn < sn_prev) {
                e.regs[0xF] = 1;
            }
            else {
                e.regs[0xF] = 0;
            }
            return sn;
        case 5:
            // 8XY5: set VX to VX - VY
            // set VF = 0 if `borrow` and 1 otherwise
            if (sn < tn) {
                e.regs[0xF] = 0;
            }
            else {
                e.regs[0xF] = 1;
            }
            sn -= tn;
            return sn;
        case 7:
            // 8XY7: set VX to VY - VX
            sn = tn - sn;
            return sn;
        
    }
}

int exec (Emulator &e, uint16_t instr) {
    uint16_t fn  = 0b0000'0000'0000'1111 & instr;    // first nibble to obtain op
    uint16_t sn  = 0b0000'0000'1111'0000 & instr;    // second nibble to find first reg
    uint16_t tn  = 0b0000'1111'0000'0000 & instr;    // third nibble to find second reg
    uint16_t pn  = 0b1111'0000'0000'0000 & instr;    // fourth nibble

    switch (fn) {
        case 0:
            // 00E0: clear display
            if (instr == 0x00E0) {
                // ???????
            }
            // 00EE: return from subroutine
            else if (instr == 0x00EE) {
                for (auto i = 0; i != STACKSIZE; ++i) {
                    if (e.stack[i] != 0xFFFF) {
                        // set PC equal to last address
                        e.PC = (char*)(e.stack[i]);
                        // pop last address from stack
                        e.stack[i] = 0xFFFF;
                        break;
                    }
                }
            }
            break;
        case 1:
            // 1NNN: jump
            e.PC = (char*)(sn + tn + pn);
            break;
        case 2:
            // 2NNN: call subroutine
            int i = findstackspace(e);
            assert(i != -1);
            // push PC to stack
            e.stack[i] = (uint16_t)e.PC;
            // set PC to NNN
            e.stack[i] = sn + tn + pn;
            break;
        case 3:
            // 3XNN: skip one instruction if VX == NN
            if (sn == tn + pn) {
                e.PC += 2;
            }
            break;
        case 4:
            // 4XNN: skip one instruction if VX != NN
            if (sn != tn + pn) {
                e.PC += 2;
            }
            break;
        case 5:
            // 5XY0: skip one instruction if VX == VY
            if (sn == tn) {
                e.PC += 2;
            }
            break;
        case 6:
            // 6XNN: set VX to NN
            sn = tn + pn;
            break;
        case 7:
            // 7XNN: add NN to VX
            // overflow does not change VF
            sn += tn + pn;
            break;
        case 8:

            break;
        case 9:
            // 9XY0: skip one instruction if VX != VY
            if (sn != tn) {
                e.PC += 2;
            }
            break;
        case 0xA:
            break;
        case 0xB:
            break;
        case 0xC:
            break;
        case 0xD:
            break;
        case 0xE:
            break;
        case 0xF:
            break;
        default:
            break;
    }

    return 0;
}

int main () {

    Emulator e;
    // copy font data to 0x050-0x09F in MEMBUF
    uint16_t dest = (uint16_t)(&e.membuf[0]) + 0x050;
    memcpy((void*)dest, (void*)(&e.fontdata[0]), 0x09F-0x050+1);



    return 0;
}