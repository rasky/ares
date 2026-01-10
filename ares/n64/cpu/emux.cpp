// emux - emulator extensions for homebrew developers

auto CPU::XDETECT(r64& rd) -> void {
    if(!system.homebrewMode) return;
    n64 detect = 0;
    detect.bit(0) = 1;  // XDETECT
    detect.bit(5) = 1;  // XLOG
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
