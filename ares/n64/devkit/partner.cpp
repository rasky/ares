auto DevkitPartner::power(bool) -> void {
  io.data = 0;
  transfer.handshakePending = false;
  transfer.handshakeMode = 0;
  transfer.handshakeLength = 0;
  transfer.writeActive = false;
  transfer.writeExpected = 0;
  transfer.writeOffset = 0;
  for(auto& byte : transfer.writeData) byte = 0;
  transfer.readActive = false;
  transfer.readOffset = 0;
  transfer.readSource = ReadSourceNone;
  for(auto& transferData : response.data) {
    for(auto& byte : transferData) byte = 0;
  }
  for(auto& size : response.size) size = 0;
  response.read = 0;
  response.write = 0;
  response.count = 0;
  streamReset();
  for(auto& word : queue.data) word = 0;
  queue.read = 0;
  queue.write = 0;
  queue.count = 0;
}

auto DevkitPartner::queueWord(u32 data_) -> void {
  if(queue.count >= 64) return;
  queue.data[queue.write] = data_;
  queue.write = (queue.write + 1) & 63;
  queue.count++;
}

auto DevkitPartner::queueTransfer(const u8* data_, u32 size) -> bool {
  if(response.count >= ResponseQueueSize) return false;
  if(size > TransferChunkSize) size = TransferChunkSize;
  u32 slot = response.write;
  for(u32 n : range(size)) response.data[slot][n] = data_[n];
  response.size[slot] = size;
  response.write = (response.write + 1) % ResponseQueueSize;
  response.count++;
  return true;
}

auto DevkitPartner::queueU32(u32 data_) -> bool {
  u8 payload[4] = {
    (u8)(data_ >> 24), (u8)(data_ >> 16), (u8)(data_ >> 8), (u8)(data_ >> 0)
  };
  return queueTransfer(payload, 4);
}

auto DevkitPartner::streamReset() -> void {
  stream.active = false;
  stream.name = {};
  stream.file = {};
  stream.size = 0;
  stream.offset = 0;
  stream.chunkSize = 0;
  for(auto& byte : stream.chunk) byte = 0;
}

auto DevkitPartner::streamPrepareChunk() -> bool {
  if(!stream.active) return false;
  if(stream.chunkSize) return true;
  if(stream.offset >= stream.size) {
    streamReset();
    return false;
  }

  if(!stream.file) {
    auto fp = file::open(stream.name, file::mode::read);
    if(!fp) {
      streamReset();
      return false;
    }
    stream.file = std::move(fp);
    stream.file.seek(stream.offset, file::index::absolute);
  }

  u32 size = min<u32>(TransferChunkSize, stream.size - stream.offset);
  stream.file.read({stream.chunk, size});
  stream.chunkSize = size;
  stream.offset += size;
  return true;
}

auto DevkitPartner::hostResolvePath(const string& name) -> string {
  auto probe = [&](const string& base) -> string {
    if(!base) return {};
    string directory = base;
    if(file::exists(directory)) directory = Location::dir(directory);
    if(!directory.endsWith("/")) directory.append("/");
    string candidate{directory, name};
    if(file::exists(candidate)) return candidate;
    return {};
  };

  if(file::exists(name)) return name;
  if(auto filename = probe(cartridge.pak ? cartridge.pak->attribute("location") : "")) return filename;
  if(_DD() && dd.pak) {
    if(auto filename = probe(dd.pak->attribute("location"))) return filename;
  }
  if(auto filename = probe(Path::active())) return filename;
  return {};
}

auto DevkitPartner::consumeCommand() -> void {
  u8 command = transfer.writeData[0];
  string path;
  for(u32 n = 1; n < transfer.writeExpected; n++) {
    u8 c = transfer.writeData[n];
    if(!c) break;
    path.append((char)c);
  }

  if(command == 0x42) {
    streamReset();

    auto filename = hostResolvePath(path);
    if(!filename) {
      devkit.debugger.notify({"[Partner64] file request missing: ", path});
      queueU32(0x0100'0000);
      queueU32(0);
      return;
    }

    auto fp = file::open(filename, file::mode::read);
    if(!fp) {
      devkit.debugger.notify({"[Partner64] file request open failed: ", path});
      queueU32(0x0100'0000);
      queueU32(0);
      return;
    }

    u32 size32 = (u32)fp.size();
    if(!queueU32(0x0000'0000) || !queueU32(size32)) {
      devkit.debugger.notify("[Partner64] response queue full while opening file");
      queueU32(0x0100'0000);
      queueU32(0);
      return;
    }

    stream.active = true;
    stream.name = filename;
    stream.file = std::move(fp);
    stream.size = size32;
    stream.offset = 0;
    stream.chunkSize = 0;
    for(auto& byte : stream.chunk) byte = 0;

    devkit.debugger.notify({"[Partner64] file opened(stream): ", path, " bytes=", size32});
    return;
  }

  if(command == 0x49) {
    auto filename = hostResolvePath(path);
    u32 result = filename ? 0x0000'0000 : 0x0100'0000;
    queueU32(result);
    if(filename) {
      devkit.debugger.notify({"[Partner64] path probe hit: ", path, " => ", filename});
    } else {
      devkit.debugger.notify({"[Partner64] path probe miss: ", path});
    }
    return;
  }

  if(command == 0x44) {
    devkit.debugger.notify("[Partner64] upload request is not implemented");
    return;
  }

  devkit.debugger.notify({"[Partner64] unknown command 0x", hex(command, 2L), " path=", path});
  u8 result[4] = {0x00, 0x00, 0x00, 0x00};
  queueTransfer(result, 4);
}

auto DevkitPartner::status() -> u32 {
  u32 status = 0x4;
  if(queue.count || transfer.readActive || response.count || stream.active) status |= 0x2;
  return status;
}

auto DevkitPartner::transferWriteWord(u32 data_) -> void {
  if(data_ == 0x82 && !transfer.handshakePending && !transfer.writeActive) {
    bool hasResponse = false;

    if(response.count) {
      hasResponse = true;
      transfer.readSource = ReadSourceResponse;
      transfer.handshakeLength = response.size[response.read];
    } else if(streamPrepareChunk()) {
      hasResponse = true;
      transfer.readSource = ReadSourceStream;
      transfer.handshakeLength = stream.chunkSize;
    } else {
      transfer.readSource = ReadSourceNone;
      transfer.handshakeLength = 0x100;
    }

    transfer.handshakePending = true;
    transfer.handshakeMode = hasResponse ? 2 : 3;
    queueWord(transfer.handshakeMode);
    queueWord(transfer.handshakeLength);
    return;
  }

  if(transfer.handshakePending) {
    u32 negotiated = data_;
    if(negotiated == transfer.handshakeLength) {
      if(transfer.handshakeMode == 2) {
        transfer.readActive = true;
        transfer.readOffset = 0;
      }
      if(transfer.handshakeMode == 3) {
        transfer.writeActive = true;
        transfer.writeExpected = negotiated;
        transfer.writeOffset = 0;
      }
    } else {
      devkit.debugger.notify({
        "[Partner64] transfer negotiation mismatch mode=", transfer.handshakeMode,
        " expected=", transfer.handshakeLength, " got=", negotiated
      });
    }
    transfer.handshakePending = false;
    return;
  }

  if(transfer.writeActive) {
    u32 offset = transfer.writeOffset;
    if(offset + 0 < sizeof(transfer.writeData) && offset + 0 < transfer.writeExpected) {
      transfer.writeData[offset + 0] = data_ >> 24;
    }
    if(offset + 1 < sizeof(transfer.writeData) && offset + 1 < transfer.writeExpected) {
      transfer.writeData[offset + 1] = data_ >> 16;
    }
    if(offset + 2 < sizeof(transfer.writeData) && offset + 2 < transfer.writeExpected) {
      transfer.writeData[offset + 2] = data_ >> 8;
    }
    if(offset + 3 < sizeof(transfer.writeData) && offset + 3 < transfer.writeExpected) {
      transfer.writeData[offset + 3] = data_ >> 0;
    }
    transfer.writeOffset += 4;
    if(transfer.writeOffset >= transfer.writeExpected) {
      transfer.writeActive = false;
      consumeCommand();
    }
    return;
  }

  io.data = data_;
}

auto DevkitPartner::transferReadWord() -> u32 {
  u32 data_ = 0;
  if(queue.count) {
    data_ = queue.data[queue.read];
    queue.read = (queue.read + 1) & 63;
    queue.count--;
  } else if(transfer.readActive) {
    u32 size = 0;
    u32 offs = transfer.readOffset;
    u8 b0 = 0, b1 = 0, b2 = 0, b3 = 0;

    if(transfer.readSource == ReadSourceResponse && response.count) {
      u32 slot = response.read;
      size = response.size[slot];
      b0 = offs + 0 < size ? response.data[slot][offs + 0] : 0;
      b1 = offs + 1 < size ? response.data[slot][offs + 1] : 0;
      b2 = offs + 2 < size ? response.data[slot][offs + 2] : 0;
      b3 = offs + 3 < size ? response.data[slot][offs + 3] : 0;
    }

    if(transfer.readSource == ReadSourceStream && stream.chunkSize) {
      size = stream.chunkSize;
      b0 = offs + 0 < size ? stream.chunk[offs + 0] : 0;
      b1 = offs + 1 < size ? stream.chunk[offs + 1] : 0;
      b2 = offs + 2 < size ? stream.chunk[offs + 2] : 0;
      b3 = offs + 3 < size ? stream.chunk[offs + 3] : 0;
    }

    if(!size) {
      transfer.readActive = false;
      transfer.readOffset = 0;
      transfer.readSource = ReadSourceNone;
      io.data = data_;
      return data_;
    }

    data_ = (u32)b0 << 24 | (u32)b1 << 16 | (u32)b2 << 8 | (u32)b3 << 0;
    transfer.readOffset += 4;

    if(transfer.readOffset >= size) {
      transfer.readActive = false;
      transfer.readOffset = 0;

      if(transfer.readSource == ReadSourceResponse && response.count) {
        response.read = (response.read + 1) % ResponseQueueSize;
        response.count--;
      }

      if(transfer.readSource == ReadSourceStream) {
        stream.chunkSize = 0;
        if(stream.offset >= stream.size) streamReset();
      }

      transfer.readSource = ReadSourceNone;
    }
  }
  io.data = data_;
  return data_;
}

auto DevkitPartner::readWord(u32 address) -> u32 {
  address &= 0x1f;
  if(address == 0x00) return transferReadWord();
  if(address == 0x04) return status();
  return 0;
}

auto DevkitPartner::writeWord(u32 address, u32 data_) -> void {
  address &= 0x1f;
  if(address == 0x00) return transferWriteWord(data_);
  if(address == 0x04) return;
}
