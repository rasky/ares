// emux - emulator extensions for homebrew developers

auto CPU::XDETECT(r64& rd) -> void {
    if(!system.homebrewMode) return;
    n64 detect = 0;
    detect.bit(0x20) = 1;  // XDETECT
    detect.bit(0x25) = 1;  // XLOG
    detect.bit(0x27) = 1;  // XHEXDUMP
    detect.bit(0x28) = 1;  // XPROF
    detect.bit(0x29) = 1;  // XPROFREAD
    detect.bit(0x2c) = 1;  // XIOCTL
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

auto CPU::XPROF(cr64& rd, u64 code) -> void {
    if(!system.homebrewMode) return;

    if(code == 4) {  //reset profiling data
        profileSlots.clear();
        return;
    }

    u64 slot = rd.u64;
    if (slot >= 1024) return;
    if(profileSlots.size() <= slot) {
        profileSlots.resize(slot + 1);
    }

    if(code == 1) { //start profiling
        for (int i=0; i<sizeof(profile.data)/sizeof(profile.data[0]); i++) {
            profileSlots[slot].profile.data[i] -= profile.data[i];
        }
        profileSlots[slot].started = 1;
    }
    if(code == 2) { //stop profiling
        for (int i=0; i<sizeof(profile.data)/sizeof(profile.data[0]); i++) {
            profileSlots[slot].profile.data[i] += profile.data[i];
        }
        profileSlots[slot].started = 0;
    }
    if(code == 3) { //clear profiling data
        profileSlots[slot] = {};
    }
}

auto CPU::XPROFREAD(cr64& rd, r64& rt) -> void {
    if(!system.homebrewMode) return;

    i64 slot = (i64)rd.u64;
    if (slot >= profileSlots.size()) {
        rt.u64 = 0;
        return;
    }
    auto& prof = (slot < 0 ? profile : profileSlots[slot].profile);

    u64 code = rt.u64;
    switch(code) {
        case 0x0000: rt.u64 = prof.cpuCycles; break;
        case 0x0001: rt.u64 = prof.cpuCyclesExc; break;
        case 0x0010: rt.u64 = prof.icacheHits; break;
        case 0x0011: rt.u64 = prof.icacheMisses; break;
        case 0x0012: rt.u64 = prof.icacheWritebacks; break;
        case 0x0020: rt.u64 = prof.dcacheHits; break;
        case 0x0021: rt.u64 = prof.dcacheMisses; break;
        case 0x0022: rt.u64 = prof.dcacheWritebacks; break;
        default:     rt.u64 = 0; break;
    }
}

auto CPU::XIOCTL(u64 code) -> void {
    if(!system.homebrewMode) return;

    switch(code) {
        case 0x1: //exit
            printf("[emux] Ares exit requested by application\n");
            platform->event(ares::Event::Shutdown);
            break;
    }
}   