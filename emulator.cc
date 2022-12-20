#include "emulator.hh"

#define MEMSIZE             4096
#define STACKSIZE           32
#define NREGISTERS          17      // 16 general + 1 index
#define ROM_START_ADDR      0x200

#define DISPLAY_WIDTH       64
#define DISPLAY_HEIGHT      32

#define SIZEOF(arr)         sizeof(arr) / sizeof(arr[0]);

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
    uint16_t regs [NREGISTERS] {
        0, 0, 0, 0, // V0-V3
        0, 0, 0, 0, // V4-V7
        0, 0, 0, 0, // V8-VB
        0, 0, 0, 0, // VC-VF 
        0           // VI  
    };
    char regs_valkeys [NREGISTERS-1] {
        // values are the indices
        'x', '1', '2', '3',
        'q', 'w', 'e', 'a',
        's', 'd', 'z', 'c',
        '4', 'r', 'f', 'v'
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

uint16_t findfontaddress (uint16_t regval) {
    return 0x500 + regval * 5;
}

int parse_8NNN (Emulator &e, uint16_t instr) {
    uint16_t sn  = 0b0000'0000'1111'0000 & instr;    // second nibble to find first reg
    uint16_t tn  = 0b0000'1111'0000'0000 & instr;    // third nibble to find second reg
    uint16_t pn  = 0b1111'0000'0000'0000 & instr;    // fourth nibble
    switch (pn) {
        case 0:
            // 8XY0: set VX to VY   
            e.regs[sn] = e.regs[tn];
            break;
        case 1:
            // 8XY1: set VX to VX | VY
            e.regs[sn] = e.regs[sn] | e.regs[tn];
            break;
        case 2:
            // 8XY2: set VX to VX & VY
            e.regs[sn] = e.regs[sn] & e.regs[tn];
            break;
        case 3:
            // 8XY3: set VX to VX ^ VY
            e.regs[sn] = e.regs[sn] ^ e.regs[tn];
            break;
        case 4:
            // 8XY4: set VX to VX + VY
            uint16_t vx_prev = e.regs[sn];
            e.regs[sn] += e.regs[tn];
            // set VF = 1 if 'carry' (overflow) and 0 otherwise
            if (e.regs[sn] < vx_prev) {
                e.regs[0xF] = 1;
            }
            else {
                e.regs[0xF] = 0;
            }
            break;
        case 5:
            // 8XY5: set VX to VX - VY
            // set VF = 0 if `borrow` and 1 otherwise
            if (e.regs[sn] < e.regs[tn]) {
                e.regs[0xF] = 0;
            }
            else {
                e.regs[0xF] = 1;
            }
            e.regs[sn] -= e.regs[tn];
            break;
        case 6:
            e.regs[sn] = e.regs[tn];
            // 8XY6: set VX to VY, then right bitshift VX and
            //       set VF equal to the value bitshifted out
            if (pn == 6) {
                // set VF to right-bitshifted out
                e.regs[0xF] = instr & 0b0000'0000'0000'0001;
                e.regs[sn] = e.regs[sn] >> 1;
            }
            // 8XYE: set VX to VY, then left bitshift VX and
            //       set VF equal to the value bitshifted out
            else if (pn == 0xE) {
                // set VF to bit-shifted out
                e.regs[0xF] = instr & 0b1000'0000'0000'0000;
                e.regs[sn] = e.regs[sn] << 1;
            }
            break;
        case 7:
            // 8XY7: set VX to VY - VX
            e.regs[sn] = e.regs[tn] - e.regs[sn];
            break;
        default:
            // invalid instruction
            return -1;
    }
    return 0;
}

int parse_FNNN (Emulator &e, uint16_t instr) {
    uint16_t sn  = 0b0000'0000'1111'0000 & instr;    // second nibble to find first reg
    uint16_t tn  = 0b0000'1111'0000'0000 & instr;    // third nibble to find second reg
    uint16_t pn  = 0b1111'0000'0000'0000 & instr;    // fourth nibble

    // FX07: set VX to current val of delay timer
    if (tn == 0 && pn == 7) {

    }
    // FX15: set delay timer to VX
    else if (tn == 1 && pn == 5) {

    }
    // FX18: set sound timer to VX
    else if (tn == 1 && pn == 8) {

    }
    // FX1E: set I to I+VX and set carry flag if I>1000
    else if (tn == 1 && pn = 0xE) {
        e.regs[0x11] += e.regs[sn];
        if (e.regs[0x11] > 1000) {
            e.regs[0xF] = 1;
        }
    }
    // FX0A: blocks until key is pressed, then when
    //       key is pressed, store its hex in VX
    else if (tn == 0 && pn = 0xA) {
        // decrement initially
        e.PC -= 2;
        char in;
        while (true) {
            in = check_input();
            if (in && check_keyboard(in)) {
                break;
            }
        }
        // increment so no net change if key pressed
        e.PC += 2;
        // store hex val (index in e.regs_valkeys) of key in VX
        for (auto i = 0; i != NREGISTERS - 1; ++i) {
            if (in == e.regs_valkeys[i]) {
                e.regs[sn] = i;
            }
        }
    }
    // FX29: 
    else if (tn == 2 && pn == 9) {
        e.regs[0x11] = findfontaddress(e.regs[sn]);
    }
    // FX33: store digits of VX (decimal form)
    //       at addresses I, I+1, I+2
    else if (tn == 3 && pn == 3) {

    }
    else {
        return -1;
    }
    return 0;
}

char check_input() {
    char key;
    int r = scanf(&key);
    assert(r == 0);
    return key;
}

int check_keyboard (char &key) {
    return
        (
        key == '1' || key == '2' || key == '3' || key == '4' ||
        key == 'q' || key == 'w' || key == 'e' || key == 'r' ||
        key == 'a' || key == 's' || key == 'd' || key == 'f' ||
        key == 'z' || key == 'x' || key == 'c' || key == 'v'
        );
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
            if (e.regs[sn] == tn + pn) {
                e.PC += 2;
            }
            break;
        case 4:
            // 4XNN: skip one instruction if VX != NN
            if (e.regs[sn] != tn + pn) {
                e.PC += 2;
            }
            break;
        case 5:
            // 5XY0: skip one instruction if VX == VY
            if (e.regs[sn] == e.regs[tn]) {
                e.PC += 2;
            }
            break;
        case 6:
            // 6XNN: set VX to NN
            e.regs[sn] = tn + pn;
            break;
        case 7:
            // 7XNN: add NN to VX
            // overflow does not change VF
            e.regs[sn] += (tn + pn);
            break;
        case 8:
            // 8NNN: parse externally
            int r = parse_8NNN(e, instr);
            assert(r == 0);
            break;
        case 9:
            // 9XY0: skip one instruction if VX != VY
            if (e.regs[sn] != e.regs[tn]) {
                e.PC += 2;
            }
            break;
        case 0xA:
            // ANNN: set I to NNN
            e.regs[0x11] = sn + tn + pn;
            break;
        case 0xB:
            // BNNN: jump to address (XNN + VX)
            e.PC = (char*)(sn + tn + pn + e.regs[sn]);
            break;
        case 0xC:
            // CXNN: generates random number r, then 
            // set VX to r & NN
            e.regs[sn] = (uint16_t)rand() & (sn + tn);
            break;
        case 0xD:
            uint16_t x_coord = e.regs[sn] % DISPLAY_WIDTH;
            uint16_t y_coord = e.regs[tn] % DISPLAY_HEIGHT;
            e.regs[0xF] = 0;
            // do other display stuff idk
            break;
        case 0xE:
            char in = check_input();
            if (!check_keyboard(in)) {
                break;
            }
            // EX9E: skip one instruction if key corresponding
            //       to the value in VX is pressed
            if (tn == 9 && pn == 0xE) {                
                if (in == e.regs_valkeys[e.regs[sn]]) {
                    e.PC += 2;
                }
            }
            // EXA1: skip one instruction if key corresponding
            //       to the value in VX is not pressed
            else if (tn == 0xA && pn == 1) {
                if (in != e.regs_valkeys[e.regs[sn]]) {
                    e.PC += 2;
                }
            }
            break;
        case 0xF:
            int r = parse_FNNN(e, instr); 
            assert(r == 0);
            break;
        default:
            return -1;
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