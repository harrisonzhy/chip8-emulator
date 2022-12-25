#include "emulator.hh"
#include <SDL2/SDL.h>


int fetch (Emulator &e, uint16_t i) {
    // combine two 8-bit instructions into a 16-bit instruction
    uint16_t instr = e.membuf[i] << 8 | e.membuf[i+1];
    // read 16-bit instruction
    int r = exec(e, instr);
    if (r != 0) {
        return -1;
    }
    e.PC += 2;
    return 0;
}


int exec (Emulator &e, uint16_t instr) {
    uint16_t fn  = (0xF000 & instr) >> 12;   // first nibble to obtain op
    uint16_t sn  = (0xF00 & instr) >> 8;     // second nibble to find first reg
    uint16_t tn  = (0xF0 & instr) >> 4;      // third nibble to find second reg
    uint16_t pn  = (0xF & instr);            // fourth nibble

    uint16_t nn  = 0x00FF & instr;           // 8-bit number
    uint16_t nnn = 0x0FFF & instr;           // 12-bit address

    switch (fn) {
        case 0x0: {
            // 00E0: clear display
            if (instr == 0x00E0) {
                for (auto i = 0; i < DISPLAY_HEIGHT; ++i) {
                    for (auto j = 0; j < DISPLAY_WIDTH; ++j) {
                        e.display[i][j] = 0;
                    }
                }
            }
            // 00EE: return from subroutine
            else if (instr == 0x00EE) {
                for (auto i = 0; i != STACKSIZE; ++i) {
                    if (e.stack[i] != 0xFFFF) {
                        // set PC equal to index of last address
                        int r = findpc(e, e.stack[i]);
                        assert(r >= 0);
                        e.PC = r;
                        // pop last address from stack
                        e.stack[i] = 0xFFFF;
                        break;
                    }
                }
            }
            break;
        }
        case 0x1: {
            // 1NNN: jump
            e.membuf[e.PC] = nnn;
            int s = findpc(e, nnn);
            assert(s >= 0);
            e.PC = s;
            break;
        }
        case 0x2: {
            // 2NNN: call subroutine
            int i = findstackspace(e);
            assert(i >= 0);
            // push PC to stack
            e.stack[i] = e.membuf[e.PC];
            // set PC to NNN
            e.membuf[e.PC] = nnn;
            break;
        }
        case 0x3: {
            // 3XNN: skip one instruction if VX == NN
            if (e.regs[sn] == nn) {
                e.PC += 2;
            }
            break;
        }
        case 0x4: {
            // 4XNN: skip one instruction if VX != NN
            if (e.regs[sn] != nn) {
                e.PC += 2;
            }
            break;
        }
        case 0x5: {
            // 5XY0: skip one instruction if VX == VY
            if (e.regs[sn] == e.regs[tn]) {
                e.PC += 2;
            }
            break;
        }
        case 0x6: {
            // 6XNN: set VX to NN
            e.regs[sn] = nn;
            break;
        }
        case 0x7: {
            // 7XNN: add NN to VX
            // overflow does not change VF
            e.regs[sn] += nn;
            break;
        }
        case 0x8: {
            // 8NNN: parse externally
            int r = parse_8NNN(e, instr);
            assert(r == 0);
            break;
        }
        case 0x9: {
            // 9XY0: skip one instruction if VX != VY
            if (e.regs[sn] != e.regs[tn]) {
                e.PC += 2;
            }
            break;
        }
        case 0xA: {
            // ANNN: set I to NNN
            e.membuf[e.I] = nnn;
            break;
        }
        case 0xB: {
            // BNNN: jump to address (NNN + VX)
            int t = findpc(e, nnn + e.regs[sn]);
            assert(t >= 0);
            e.PC = t;
            break;
        }
        case 0xC: {
            // CXNN: generates random number rn in [0, NN], then 
            // set VX to rn & NN
            int rn = std::rand() % (nn + 1) + nn;
            e.regs[sn] = rn & nn;
            break;
        }
        case 0xD: {
            // DXYN: Draws an N-pixel tall sprite from where I is
            // currently pointing on the screen, as well as from
            // VX and VY on the screen

            // find coordinates
            uint16_t x = e.regs[sn] % DISPLAY_WIDTH;
            uint16_t y = e.regs[tn] % DISPLAY_HEIGHT;
            e.regs[0xF] = 0;

            assert(x < DISPLAY_WIDTH);
            assert(y < DISPLAY_HEIGHT);

            // handle pixel updates
            for (auto i = 0; i != pn; ++i) {
                for (auto j = 0; j != 8; ++j) {
                    // if current pixel is on and pixel at
                    // (x,y) is also on, then turn off pixel at (x,y)
                    // and set VF to 1
                    uint16_t p = e.membuf[e.I + j];
                    if ((p & (0x80 >> j)) == 1 && e.display[y][x] == 1) {
                        e.display[y][x] = 0;
                        e.regs[0xF] = 1;
                    }
                    // if current pixel is on and pixel at
                    // (x,y) is off, then turn on pixel at (x,y)
                    // and leave VF as 0
                    else if ((p & (0x80 >> j)) == 1 && e.display[x][y] == 0) {
                        e.display[y][x] = 1;
                    }
                }
            }
            break;
        }
        case 0xE: {
            char in = check_input();
            if (!check_keyboard(in)) {
                break;
            }
            // EX9E: skip one instruction if key corresponding
            //       to the value in VX is pressed
            if (tn == 0x9 && pn == 0xE) {                
                if (in == e.regs_valkeys[e.regs[sn]]) {
                    e.PC += 2;
                }
            }
            // EXA1: skip one instruction if key corresponding
            //       to the value in VX is not pressed
            else if (tn == 0xA && pn == 0x1) {
                if (in != e.regs_valkeys[e.regs[sn]]) {
                    e.PC += 2;
                }
            }
            break;
        }
        case 0xF: {
            int s = parse_FNNN(e, instr);
            printf("%u\n", instr);
            assert(s == 0);
            break;
        }
        default: {
            // invalid instruction
            return -1;
        }
    }
    return 0;
}


int parse_8NNN (Emulator &e, uint16_t instr) {
    uint16_t sn  = (0xF00 & instr) >> 8;     // second nibble to find first reg
    uint16_t tn  = (0xF0 & instr) >> 4;      // third nibble to find second reg
    uint16_t pn  = (0xF & instr);            // fourth nibble

    switch (pn) {
        case 0x0: {
            // 8XY0: set VX to VY   
            e.regs[sn] = e.regs[tn];
            break;
        }
        case 0x1: {
            // 8XY1: set VX to VX | VY
            e.regs[sn] = e.regs[sn] | e.regs[tn];
            break;
        }
        case 0x2: {
            // 8XY2: set VX to VX & VY
            e.regs[sn] = e.regs[sn] & e.regs[tn];
            break;
        }
        case 0x3: {
            // 8XY3: set VX to VX ^ VY
            e.regs[sn] = e.regs[sn] ^ e.regs[tn];
            break;
        }
        case 0x4: {
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
        }
        case 0x5: {
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
        }
        case 0x6: {
            e.regs[sn] = e.regs[tn];
            // 8XY6: set VX to VY, then right bitshift VX and
            //       set VF equal to the value bitshifted out
            if (pn == 0x6) {
                // set VF to right-bitshifted out
                e.regs[0xF] = instr & 0b1;
                e.regs[sn] = e.regs[sn] >> 1;
            }
            // 8XYE: set VX to VY, then left bitshift VX and
            //       set VF equal to the value bitshifted out
            else if (pn == 0xE) {
                // set VF to bit-shifted out
                e.regs[0xF] = instr & 0x8000;
                e.regs[sn] = e.regs[sn] << 1;
            }
            break;
        }
        case 0x7: {
            // 8XY7: set VX to VY - VX
            e.regs[sn] = e.regs[tn] - e.regs[sn];
            break;
        }
        default: {
            return -1;
        }
    }
    return 0;
}


int parse_FNNN (Emulator &e, uint16_t instr) {
    uint16_t sn  = (0xF00 & instr) >> 8;     // second nibble to find first reg
    uint16_t tn  = (0xF0 & instr) >> 4;      // third nibble to find second reg
    uint16_t pn  = (0xF & instr);            // fourth nibble

    // FX07: set VX to current val of delay timer
    if (tn == 0x0 && pn == 0x7) {
        e.regs[sn] = e.delay_timer;
    }
    // FX15: set delay timer to VX
    else if (tn == 0x1 && pn == 0x5) {
        e.delay_timer = e.regs[sn];
    }
    // FX18: set sound timer to VX
    else if (tn == 0x1 && pn == 0x8) {
        e.sound_timer = e.regs[sn];
    }
    // FX1E: set I to I+VX and set carry flag if overflow
    else if (tn == 0x1 && pn == 0xE) {
        e.membuf[e.I] += e.regs[sn];
        // check overflow
        if (e.membuf[e.I] < e.regs[sn]) {
            e.regs[0xF] = 1;
        }
    }
    // FX0A: blocks until key is pressed, then when
    //       key is pressed, store its hex in VX
    else if (tn == 0x0 && pn == 0xA) {
        // decrement initially
        e.PC -= 2;
        assert(e.PC < MEMSIZE);
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
        for (auto i = 0; i != NREGISTERS; ++i) {
            if (in == e.regs_valkeys[i]) {
                e.regs[sn] = i;
            }
        }
    }
    // FX29:
    else if (tn == 0x2 && pn == 0x9) {
        e.I = findfontindex(e.regs[sn]);
    }
    // FX33: store digits of VX (decimal form) at I, I+1, I+2
    else if (tn == 0x3 && pn == 0x3) {
        e.membuf[e.I] = (e.regs[sn] / 100) % 10;
        e.membuf[e.I+1] = (e.regs[sn] / 10) % 10;
        e.membuf[e.I+2] = (e.regs[sn]) % 10;
    }
    // FX55: store VX at I+X for X in [0, SIZEOF(e.regs)]
    else if (tn == 0x5 && pn == 0x5) {
        for (auto i = 0; i != NREGISTERS; ++i) {
            e.membuf[e.I+i] = e.regs[i];
        }
    }
    // FX65: store the value of I+X into VX
    else if (tn == 0x6 && pn == 0x5) {
        for (auto i = 0; i != NREGISTERS; ++i) {
            e.regs[i] = e.membuf[e.I+i];
        }
    }
    else {
        return -1;
    }
    return 0;
}


int main () {
    // create emulator and display
    Emulator e;
    SDL_Init(SDL_INIT_VIDEO);
    e.window = SDL_CreateWindow("", 
                                SDL_WINDOWPOS_CENTERED, 
                                SDL_WINDOWPOS_CENTERED,
                                DISPLAY_WIDTH*TEXEL_SCALE, 
                                DISPLAY_HEIGHT*TEXEL_SCALE, 0);
    assert(e.window);
    e.renderer = SDL_CreateRenderer((SDL_Window*)e.window, -1, 
                                    SDL_RENDERER_ACCELERATED | SDL_RENDERER_TARGETTEXTURE);
    assert(e.renderer);
    SDL_RenderClear((SDL_Renderer*)e.renderer);

    // SDL_Rect recttest = {0, 0, TEXEL_SCALE, TEXEL_SCALE};
    // SDL_SetRenderDrawColor((SDL_Renderer*)e.renderer, 0xFF, 0xFF, 0xFF, SDL_ALPHA_OPAQUE);
    // for (auto i = 0; i != DISPLAY_WIDTH; ++i) {
    //     recttest.x = i*TEXEL_SCALE;
    //     recttest.y = i*TEXEL_SCALE;
    //     SDL_RenderFillRect((SDL_Renderer*)e.renderer, &recttest);
    // }
    SDL_RenderPresent((SDL_Renderer*)e.renderer);

    // load font data into 0x050-0x09F in membuf
    uintptr_t fdest = (uintptr_t)(&e.membuf[0]) + 0x050;
    memcpy((void*)fdest, (void*)(&e.fontdata[0]), 0x09F-0x050+1);    

    // load game data into 0x200-0xFFF in membuf
    std::ifstream f(GAME_PATH, std::ios::binary | std::ios::ate);
    if (!f.is_open()) {
        return -1;
    }
    std::streampos sz = f.tellg();
    char* tbuf = new char[sz];
    f.seekg(0, std::ios::beg);
    f.read(tbuf, sz);
    f.close();
    for (auto i = 0; i != sz; ++i) {
        e.membuf[ROM_START_ADDR+i] = tbuf[i];
    }
    delete[] tbuf;

    // run game
    SDL_Event s;
    SDL_Rect rect = {0, 0, TEXEL_SCALE, TEXEL_SCALE};
    while (1) {
        unsigned int loops = 0;
        while (e.PC < MEMSIZE-1) {
            // fetch and execute instruction at membuf[PC]
            int r = fetch(e, e.PC);
            assert(r == 0);

            // update display
            for (auto i = 0; i < DISPLAY_HEIGHT; ++i) {
                for (auto j = 0; j < DISPLAY_WIDTH; ++j) {
                    rect.x = j*TEXEL_SCALE;
                    rect.y = i*TEXEL_SCALE;
                    
                    // if pixel is on, draw white
                    if (e.display[j][i] == 0) {
                        SDL_SetRenderDrawColor((SDL_Renderer*)e.renderer, 0xFF, 0xFF, 0xFF, SDL_ALPHA_OPAQUE);
                        SDL_RenderFillRect((SDL_Renderer*)e.renderer, &rect);
                    }
                    // else if pixel is off, draw black
                    else if (e.display[j][i] == 1) {
                        SDL_SetRenderDrawColor((SDL_Renderer*)e.renderer, 0x00, 0x00, 0x00, SDL_ALPHA_OPAQUE);
                        SDL_RenderFillRect((SDL_Renderer*)e.renderer, &rect);
                    }
                    SDL_RenderPresent((SDL_Renderer*)e.renderer);

                    // handle quit
                    SDL_PollEvent(&s);
                    if (s.type == SDL_QUIT) {
                        goto quit;
                    }
                }
            }
            // restrict CPU cycle to ~600 Hz, update timers at 60 Hz
            //msleep(TMSLEEP);
            if (loops % 10 == 0) {
                updatedelaytimer(e);
                updatesoundtimer(e);
            }
            // handle quit
            SDL_PollEvent(&s);
            if (s.type == SDL_QUIT) {
                goto quit;
            }
            ++loops;
        }
        // handle quit
        SDL_PollEvent(&s);
        if (s.type == SDL_QUIT) {
            goto quit;
        }
    }
    goto quit;
    quit:
        SDL_DestroyRenderer((SDL_Renderer*)e.renderer);
        SDL_DestroyWindow((SDL_Window*)e.window);
        SDL_Quit();

    return 0;
}