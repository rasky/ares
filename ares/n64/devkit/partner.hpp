#pragma once

struct DevkitPartner {
  enum : u32 {
    TransferChunkSize = 0x400,
    ResponseQueueSize = 16,
    ReadSourceNone = 0,
    ReadSourceResponse = 1,
    ReadSourceStream = 2,
  };

  auto power(bool reset) -> void;
  auto readWord(u32 address) -> u32;
  auto writeWord(u32 address, u32 data) -> void;
  auto serialize(serializer& s) -> void;

private:
  auto queueWord(u32 data) -> void;
  auto queueTransfer(const u8* data, u32 size) -> bool;
  auto consumeCommand() -> void;
  auto transferWriteWord(u32 data) -> void;
  auto transferReadWord() -> u32;
  auto status() -> u32;
  auto queueU32(u32 data) -> bool;
  auto hostResolvePath(const string& name) -> string;
  auto streamReset() -> void;
  auto streamPrepareChunk() -> bool;

  struct IO {
    u32 data = 0;
  } io;

  struct Transfer {
    bool handshakePending = false;
    u32 handshakeMode = 0;
    u32 handshakeLength = 0;
    bool writeActive = false;
    u32 writeExpected = 0;
    u32 writeOffset = 0;
    u8 writeData[0x400]{};
    bool readActive = false;
    u32 readOffset = 0;
    u32 readSource = ReadSourceNone;
  } transfer;

  struct Response {
    u8 data[ResponseQueueSize][TransferChunkSize]{};
    u32 size[ResponseQueueSize]{};
    u32 read = 0;
    u32 write = 0;
    u32 count = 0;
  } response;

  struct Stream {
    bool active = false;
    string name;
    file_buffer file;
    u32 size = 0;
    u32 offset = 0;
    u32 chunkSize = 0;
    u8 chunk[TransferChunkSize]{};
  } stream;

  struct Queue {
    u32 data[64]{};
    u32 read = 0;
    u32 write = 0;
    u32 count = 0;
  } queue;
};
