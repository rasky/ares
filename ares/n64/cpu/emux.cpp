// emux - emulator extensions for homebrew developers

auto CPU::XDETECT(r64& rd) -> void {
    if(!system.homebrewMode) return;
    n64 detect = 0;
    detect.bit(0x20) = 1;  // XDETECT
    detect.bit(0x25) = 1;  // XLOG
    detect.bit(0x27) = 1;  // XHEXDUMP
    rd.u64 = detect;
}

auto CPU::XLOG(cr64& rd, cr64& rt) -> void {
    if(!system.homebrewMode) return;

    auto& emux = debugger.tracer.emux;
    u64 vaddr = rd.u64;
    if (rt.u64 == 0) {
        while (1) {
            char ch = readDebug<Byte>(vaddr++);
            if(!ch) break;
            emux->notify(ch);
        }        
    } else {
        for(u64 n = 0; n < rt.u64; n++) {
            char ch = readDebug<Byte>(vaddr++);
            emux->notify(ch);
        }
    }
}

auto CPU::XHEXDUMP(cr64& rd, cr64& rt) -> void {
    if(!system.homebrewMode) return;

    auto& emux = debugger.tracer.emux;
    u64 vaddr = rd.u64;
    u64 length = rt.u64 ? rt.u64 : 256;
    string dump;

    for(u64 n = 0; n < length; n += 16) {
        dump.append(string{hex(vaddr + n, 16L), " ", hex(vaddr + n - rd.u64, 4L), ": "});
        u8 mem[16]; u64 l = length - n < 16 ? length - n : 16;
        for(u64 m = 0; m < l; m++)  mem[m] = readDebug<Byte>(vaddr + n + m);
        for(u64 m = 0; m < 16; m++) {
            if(m < l) dump.append(string{hex(mem[m], 2L), " "});
            else       dump.append("   ");
            if(m == 7) dump.append(" ");
        } 
        dump.append(" |");
        for(u64 m = 0; m < l; ++m) {
            if(mem[m] >= 32 && mem[m] <= 126) dump.append(string{(char)mem[m]});
            else                              dump.append(".");
        }
        for(u64 m = l; m < 16; m++) dump.append(" ");
        dump.append("|\n");
    }

    emux->notify(dump);
}
