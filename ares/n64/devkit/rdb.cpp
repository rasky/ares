auto DevkitRDB::power(bool) -> void {
  io.data = 0;
  fault.offset = 0;
  for(auto& byte : fault.buffer) byte = 0;
  debug.lengthOffset = 0;
  for(auto& byte : debug.lengthBuffer) byte = 0;
  debug.offset = 0;
  debug.expected = 0;
  for(auto& byte : debug.buffer) byte = 0;
  log.countOffset = 0;
  for(auto& byte : log.countBuffer) byte = 0;
  log.offset = 0;
  log.expected = 0;
  for(auto& byte : log.buffer) byte = 0;
  for(auto& word : host.queue) word = 0;
  host.read = 0;
  host.write = 0;
  host.count = 0;
  print.size = 0;
  for(auto& c : print.buffer) c = 0;
}

auto DevkitRDB::readWord(u32 address) -> u32 {
  address &= 0xf;
  if(address == 0x0) {
    if(host.count) return host.queue[host.read];
    return io.data;
  }
  return 0;
}

auto DevkitRDB::queueHostPacket(u32 type, u32 length, u8 b0, u8 b1, u8 b2) -> void {
  if(host.count >= HostQueueSize) return;
  u32 packet = type << 26 | (length & 3) << 24 | (u32)b0 << 16 | (u32)b1 << 8 | (u32)b2 << 0;
  host.queue[host.write] = packet;
  host.write = (host.write + 1) & (HostQueueSize - 1);
  host.count++;
  cpu.scc.cause.interruptPending.bit(CPU::Interrupt::WriteRDB) = 1;
}

auto DevkitRDB::dequeueHostPacket() -> void {
  if(!host.count) return;
  host.read = (host.read + 1) & (HostQueueSize - 1);
  host.count--;
}

auto DevkitRDB::writeWord(u32 address, u32 data_) -> void {
  address &= 0xf;

  if(address == 0x0) {
    io.data = data_;
    u32 type = data_ >> 26 & 0x3f;
    u32 length = data_ >> 24 & 0x03;
    u8 payload[3]{};
    string payloadHex;
    for(u32 n : range(length)) {
      payload[n] = data_ >> (16 - n * 8) & 0xff;
      if(n) payloadHex.append(" ");
      payloadHex.append(hex(payload[n], 2L));
    }

    auto typeName = [&](u32 type) -> const char* {
      switch(type) {
      case  0: return "INVALID";
      case  1: return "GtoH_PRINT";
      case  2: return "GtoH_FAULT";
      case  3: return "GtoH_LOG_CT";
      case  4: return "GtoH_LOG";
      case  5: return "GtoH_READY_FOR_DATA";
      case  6: return "GtoH_DATA_CT";
      case  7: return "GtoH_DATA";
      case  8: return "GtoH_DEBUG";
      case  9: return "GtoH_RAMROM";
      case 10: return "GtoH_DEBUG_DONE";
      case 11: return "GtoH_DEBUG_READY";
      case 12: return "GtoH_KDEBUG";
      case 22: return "GtoH_PROF_DATA";
      }
      return "unknown";
    };

    switch(type) {
    case 1: {
      decodePrint(payload, length);
    } break;

    case 2: {
      auto decoded = decodeFault(payload, length);
      if(decoded) devkit.debugger.notify({"[RDB::fault] ", decoded, " data=", payloadHex});
    } break;

    case 3: {
      auto decoded = decodeLogCount(payload, length);
      if(decoded) devkit.debugger.notify({"[RDB::log] ", decoded, " data=", payloadHex});
    } break;

    case 4: {
      auto decoded = decodeLogData(payload, length);
      if(decoded) devkit.debugger.notify({"[RDB::log] ", decoded, " data=", payloadHex});
    } break;

    case 8: {
      auto decoded = decodeDebug(payload, length);
      if(decoded) devkit.debugger.notify({"[RDB::debug] ", decoded, " data=", payloadHex});
    } break;

    case 10: {
      if(debug.lengthOffset || debug.offset) {
        devkit.debugger.notify({"[RDB::debug] debug_done with incomplete frame"});
        debug.lengthOffset = 0;
        debug.offset = 0;
        debug.expected = 0;
      }
      devkit.debugger.notify({
        "[RDB::packet] type=", type, "(", typeName(type), ") len=", length, " data=", payloadHex
      });
    } break;

    default: {
      devkit.debugger.notify({
        "[RDB::packet] type=", type, "(", typeName(type), ") len=", length, " data=", payloadHex
      });
    } break;
    }

    cpu.scc.cause.interruptPending.bit(CPU::Interrupt::ReadRDB) = 1;
    return;
  }

  if(address == 0x8) {
    cpu.scc.cause.interruptPending.bit(CPU::Interrupt::WriteRDB) = 0;
    dequeueHostPacket();
    if(host.count) cpu.scc.cause.interruptPending.bit(CPU::Interrupt::WriteRDB) = 1;
    return;
  }

  if(address == 0xc) {
    cpu.scc.cause.interruptPending.bit(CPU::Interrupt::ReadRDB) = 0;
    return;
  }
}

auto DevkitRDB::decodePrint(const u8* payload, u32 length) -> void {
  for(u32 n : range(length)) {
    u8 c = payload[n];
    if(c == '\r') continue;
    if(c == '\n') {
      if(print.size) {
        print.buffer[print.size] = 0;
        devkit.debugger.notify({"[RDB::print] ", print.buffer});
        print.size = 0;
      }
      continue;
    }
    if(print.size >= sizeof(print.buffer) - 1) {
      print.buffer[print.size] = 0;
      devkit.debugger.notify({"[RDB::print] ", print.buffer});
      print.size = 0;
    }
    print.buffer[print.size++] = c >= 0x20 && c <= 0x7e ? c : '.';
  }
}

auto DevkitRDB::decodeLogCount(const u8* payload, u32 length) -> string {
  if(!length) return {};

  auto readU24 = [&](const u8* buffer) -> u32 {
    return (u32)buffer[0] << 16 | (u32)buffer[1] << 8 | (u32)buffer[2] << 0;
  };

  string decoded;
  for(u32 n : range(length)) {
    log.countBuffer[log.countOffset++] = payload[n];
    if(log.countOffset < sizeof(log.countBuffer)) continue;

    auto count = readU24(log.countBuffer);
    log.countOffset = 0;

    if(log.expected && log.offset < log.expected) {
      decoded.append("new_count_before_complete old=", log.expected, " recv=", log.offset, " ");
      log.expected = 0;
      log.offset = 0;
    }

    if(!count || count > LogSize) {
      decoded.append("invalid_count=0x", hex(count, 6L), " ");
      continue;
    }

    log.expected = count;
    log.offset = 0;
    decoded.append("count=", log.expected, " ");
  }

  if(decoded) {
    decoded.trimRight(" ");
    return decoded;
  }
  return {};
}

auto DevkitRDB::decodeLogData(const u8* payload, u32 length) -> string {
  if(!length) return {};

  auto readU16 = [&](const u8* buffer, u32 offset) -> u16 {
    return (u16)buffer[offset] << 8 | buffer[offset + 1];
  };

  auto readU32 = [&](const u8* buffer, u32 offset) -> u32 {
    return (u32)buffer[offset] << 24 | (u32)buffer[offset + 1] << 16
      | (u32)buffer[offset + 2] << 8 | (u32)buffer[offset + 3];
  };

  if(!log.expected) return {"data_without_count len=", length};

  for(u32 n : range(length)) {
    if(log.offset < LogSize) log.buffer[log.offset] = payload[n];
    log.offset++;
  }

  if(log.offset < log.expected) return {};

  u32 bytes = log.expected;
  u32 offset = 0;
  u32 items = 0;
  bool malformed = false;

  while(offset + 12 <= bytes) {
    u32 magic = readU32(log.buffer, offset + 0);
    u32 timestamp = readU32(log.buffer, offset + 4);
    u16 argCount = readU16(log.buffer, offset + 8);
    s16 eventId = (s16)readU16(log.buffer, offset + 10);
    u32 itemSize = ((u32)argCount + 3) * 4;
    if(argCount > 16 || offset + itemSize > bytes) {
      malformed = true;
      break;
    }

    string line{
      "item=", items, " event=", eventId, " args=", argCount,
      " time=0x", hex(timestamp, 8L), " magic=0x", hex(magic, 8L)
    };
    for(u32 n : range(argCount)) {
      u32 arg = readU32(log.buffer, offset + 12 + n * 4);
      line.append(" a", n, "=0x", hex(arg, 8L));
    }
    devkit.debugger.notify({"[RDB::logItem] ", line});

    items++;
    offset += itemSize;
  }

  if(offset != bytes) malformed = true;

  log.expected = 0;
  log.offset = 0;
  queueHostPacket(13);
  if(malformed) return {"block_malformed bytes=", bytes, " items=", items};
  return {"block_done bytes=", bytes, " items=", items};
}

auto DevkitRDB::decodeFault(const u8* payload, u32 length) -> string {
  if(!length) return {};

  auto readU16 = [&](u32 offset) -> u16 {
    return (u16)fault.buffer[offset] << 8 | fault.buffer[offset + 1];
  };

  auto readU32 = [&](u32 offset) -> u32 {
    return (u32)fault.buffer[offset] << 24 | (u32)fault.buffer[offset + 1] << 16
      | (u32)fault.buffer[offset + 2] << 8 | (u32)fault.buffer[offset + 3];
  };

  auto readU64 = [&](u32 offset) -> u64 {
    return (u64)readU32(offset) << 32 | readU32(offset + 4);
  };

  for(u32 n : range(length)) {
    if(fault.offset >= FaultSize) fault.offset = 0;
    fault.buffer[fault.offset++] = payload[n];
  }

  if(fault.offset < FaultSize) return {};

  fault.offset = 0;

  auto causeName = [&](u32 cause) -> const char* {
    switch(cause >> 2 & 0x1f) {
    case  0: return "Interrupt";
    case  1: return "TLB_Mod";
    case  2: return "TLB_Load";
    case  3: return "TLB_Store";
    case  4: return "AddrErr_Load";
    case  5: return "AddrErr_Store";
    case  6: return "BusErr_Inst";
    case  7: return "BusErr_Data";
    case  8: return "Syscall";
    case  9: return "Breakpoint";
    case 10: return "ReservedInst";
    case 11: return "Coprocessor";
    case 12: return "Overflow";
    case 13: return "Trap";
    case 15: return "FloatingPoint";
    case 23: return "Watch";
    }
    return "Unknown";
  };

  u32 priority = readU32(0x04);
  u16 state = readU16(0x10);
  u16 flags = readU16(0x12);
  u32 id = readU32(0x14);

  u64 at = readU64(0x020);
  u64 v0 = readU64(0x028);
  u64 v1 = readU64(0x030);
  u64 a0 = readU64(0x038);
  u64 a1 = readU64(0x040);
  u64 a2 = readU64(0x048);
  u64 a3 = readU64(0x050);
  u64 t0 = readU64(0x058);
  u64 t1 = readU64(0x060);
  u64 s0 = readU64(0x098);
  u64 s1 = readU64(0x0a0);
  u64 gp = readU64(0x0e8);
  u64 sp = readU64(0x0f0);
  u64 s8 = readU64(0x0f8);
  u64 ra = readU64(0x100);
  u64 lo = readU64(0x108);
  u64 hi = readU64(0x110);

  u32 sr = readU32(0x118);
  u32 pc = readU32(0x11c);
  u32 cause = readU32(0x120);
  u32 badvaddr = readU32(0x124);
  u32 rcp = readU32(0x128);
  u32 fpcsr = readU32(0x12c);

  return {
    "thread id=", id, " pri=", priority, " state=0x", hex(state, 4L), " flags=0x", hex(flags, 4L),
    " pc=0x", hex(pc, 8L), " cause=0x", hex(cause, 8L), "(", causeName(cause), ")",
    " badvaddr=0x", hex(badvaddr, 8L),
    " sr=0x", hex(sr, 8L), " fpcsr=0x", hex(fpcsr, 8L), " rcp=0x", hex(rcp, 8L),
    " at=0x", hex((u32)at, 8L), " v0=0x", hex((u32)v0, 8L), " v1=0x", hex((u32)v1, 8L),
    " a0=0x", hex((u32)a0, 8L), " a1=0x", hex((u32)a1, 8L), " a2=0x", hex((u32)a2, 8L),
    " a3=0x", hex((u32)a3, 8L), " t0=0x", hex((u32)t0, 8L), " t1=0x", hex((u32)t1, 8L),
    " s0=0x", hex((u32)s0, 8L), " s1=0x", hex((u32)s1, 8L), " gp=0x", hex((u32)gp, 8L),
    " sp=0x", hex((u32)sp, 8L), " s8=0x", hex((u32)s8, 8L), " ra=0x", hex((u32)ra, 8L),
    " lo=0x", hex((u32)lo, 8L), " hi=0x", hex((u32)hi, 8L)
  };
}

auto DevkitRDB::decodeDebug(const u8* payload, u32 length) -> string {
  if(!length) return {};

  auto readU16 = [&](const u8* buffer, u32 offset) -> u16 {
    return (u16)buffer[offset] << 8 | buffer[offset + 1];
  };

  auto readU32 = [&](const u8* buffer, u32 offset) -> u32 {
    return (u32)buffer[offset] << 24 | (u32)buffer[offset + 1] << 16
      | (u32)buffer[offset + 2] << 8 | (u32)buffer[offset + 3];
  };

  const char* replyTypeName[6]{
    "request", "reply", "exception", "thread_exit", "process_exit", "console"
  };

  const char* methodName[2]{
    "cpu", "rsp"
  };

  const char* codeName[54]{
    "LoadProgram",   "ListProcesses", "GetExeName",     "ListThreads",    "ThreadStatus",   "NotImplemented",
    "StopThread",    "RunThread",     "NotImplemented", "NotImplemented", "SetFault",       "NotImplemented",
    "GetRegionCount","GetRegions",    "GetGRegisters",  "SetGRegisters",  "GetFRegisters",  "SetFRegisters",
    "ReadMem",       "WriteMem",      "SetBreak",       "ClearBreak",     "ListBreak",      "NotImplemented",
    "NotImplemented","NotImplemented","NotImplemented", "NotImplemented", "NotImplemented", "NotImplemented",
    "SetComm",       "NotImplemented","NotImplemented", "NotImplemented", "NotImplemented", "NotImplemented",
    "NotImplemented","NotImplemented","NotImplemented", "NotImplemented", "NotImplemented", "NotImplemented",
    "NotImplemented","NotImplemented","NotImplemented", "NotImplemented", "NotImplemented", "NotImplemented",
    "NotImplemented","GetSRegs",      "SetSRegs",       "GetVRegs",       "SetVRegs",       "NotImplemented"
  };

  string decoded;
  for(u32 n : range(length)) {
    u8 byte = payload[n];

    if(debug.lengthOffset < sizeof(debug.lengthBuffer)) {
      debug.lengthBuffer[debug.lengthOffset++] = byte;
      if(debug.lengthOffset == sizeof(debug.lengthBuffer)) {
        debug.expected = readU32(debug.lengthBuffer, 0);
        debug.offset = 0;
        if(!debug.expected || debug.expected > DebugSize) {
          decoded.append("bad_length=0x", hex(debug.expected, 8L), " ");
          debug.lengthOffset = 0;
          debug.expected = 0;
        }
      }
      continue;
    }

    if(debug.offset < DebugSize) debug.buffer[debug.offset] = byte;
    debug.offset++;

    if(debug.offset < debug.expected) continue;

    if(debug.expected < 12) {
      decoded.append("frame_len=", debug.expected, " short ");
    } else {
      u32 frameLength = readU32(debug.buffer, 0);
      u8 code = debug.buffer[4];
      u8 type = debug.buffer[5];
      s16 error = (s16)readU16(debug.buffer, 6);
      u8 rev = debug.buffer[8];
      u8 method = debug.buffer[9];

      const char* typeName = type < 6 ? replyTypeName[type] : "unknown";
      const char* commandName = code < 54 ? codeName[code] : "unknown";
      const char* methodText = method < 2 ? methodName[method] : "unknown";

      decoded.append(
        "len=", debug.expected, "/0x", hex(debug.expected, 8L),
        " hdr.len=0x", hex(frameLength, 8L),
        " code=", code, "(", commandName, ")",
        " type=", type, "(", typeName, ")",
        " err=", error,
        " rev=", rev,
        " method=", method, "(", methodText, ")"
      );

      if(code == 4 && debug.expected >= 76) {
        u32 tid = readU32(debug.buffer, 20);
        u32 pid = readU32(debug.buffer, 24);
        u32 instr = readU32(debug.buffer, 28);
        u32 addr = readU32(debug.buffer, 32);
        u16 major = readU16(debug.buffer, 36);
        u16 minor = readU16(debug.buffer, 38);
        decoded.append(
          " tid=", tid, " pid=", pid, " instr=0x", hex(instr, 8L),
          " fault.addr=0x", hex(addr, 8L), " fault.major=", major, " fault.minor=", minor
        );
      }
      decoded.append(" ");
    }

    debug.lengthOffset = 0;
    debug.offset = 0;
    debug.expected = 0;
  }

  if(decoded) {
    decoded.trimRight(" ");
    return decoded;
  }
  return {};
}
