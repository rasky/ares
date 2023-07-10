
auto RSP::TNE(cr32& rs, cr32& rt, u32 code) -> void {
  if(&rs != &rt) return INVALID();
  EMUX(rt, code);
}

auto RSP::EMUX(cr32& rt, u32 code) -> void {
    switch (code) {
    case 0x20: // trace(start)
        debugger.tracer.instruction->setEnabled(true);
        debugger.tracer.instructionCountdown = 0;
        break;
    case 0x21: // trace(count)
        printf("[emux] trace(count): %08x\n", rt.u32);
        debugger.tracer.instruction->setEnabled(true);
        debugger.tracer.instructionCountdown = rt.u32;
        break;
    case 0x22: // trace(stop)
        debugger.tracer.instruction->setEnabled(false);
        break;
    default:
        printf("[emux] unknown emux code: %08x\n", code);
        break;
    }
}
