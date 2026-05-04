#pragma once

struct DevkitRDB {
  auto power(bool reset) -> void;
  auto readWord(u32 address) -> u32;
  auto writeWord(u32 address, u32 data) -> void;
  auto serialize(serializer& s) -> void;

  template<u32 Size> auto read(u32 address) -> u64 {
    static_assert(Size == Byte || Size == Half || Size == Word || Size == Dual);
    auto data = readWord(address);
    if constexpr(Size == Byte) {
      switch(address & 3) {
      case 0: return data >> 24;
      case 1: return data >> 16;
      case 2: return data >> 8;
      case 3: return data >> 0;
      }
    }
    if constexpr(Size == Half) {
      switch(address & 2) {
      case 0: return data >> 16;
      case 2: return data >> 0;
      }
    }
    if constexpr(Size == Word) return data;
    if constexpr(Size == Dual) return (u64)data << 32 | readWord(address + 4);
    return 0;
  }

  template<u32 Size> auto write(u32 address, u64 data) -> void {
    static_assert(Size == Byte || Size == Half || Size == Word || Size == Dual);
    if constexpr(Size == Byte) {
      switch(address & 3) {
      case 0: return writeWord(address, data << 24);
      case 1: return writeWord(address, data << 16);
      case 2: return writeWord(address, data << 8);
      case 3: return writeWord(address, data << 0);
      }
    }
    if constexpr(Size == Half) {
      switch(address & 2) {
      case 0: return writeWord(address, data << 16);
      case 2: return writeWord(address, data << 0);
      }
    }
    if constexpr(Size == Word) return writeWord(address, data);
    if constexpr(Size == Dual) return writeWord(address, data >> 32);
  }

private:
  auto queueHostPacket(u32 type, u32 length = 0, u8 b0 = 0, u8 b1 = 0, u8 b2 = 0) -> void;
  auto dequeueHostPacket() -> void;

  auto decodePrint(const u8* payload, u32 length) -> void;
  auto decodeLogCount(const u8* payload, u32 length) -> string;
  auto decodeLogData(const u8* payload, u32 length) -> string;
  auto decodeFault(const u8* payload, u32 length) -> string;
  auto decodeDebug(const u8* payload, u32 length) -> string;

  enum : u32 {
    FaultSize = 0x1b0,
    LogSize = 0x8000,
    DebugSize = 0x800,
    HostQueueSize = 64,
  };

  struct Fault {
    u32 offset = 0;
    u8  buffer[FaultSize]{};
  } fault;

  struct Debug {
    u32 lengthOffset = 0;
    u8  lengthBuffer[4]{};
    u32 offset = 0;
    u32 expected = 0;
    u8  buffer[DebugSize]{};
  } debug;

  struct Log {
    u32 countOffset = 0;
    u8  countBuffer[3]{};
    u32 offset = 0;
    u32 expected = 0;
    u8  buffer[LogSize]{};
  } log;

  struct Host {
    u32 queue[HostQueueSize]{};
    u32 read = 0;
    u32 write = 0;
    u32 count = 0;
  } host;

  struct Print {
    u32 size = 0;
    char buffer[1024]{};
  } print;

  struct IO {
    u32 data = 0;
  } io;
};
