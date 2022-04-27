auto Cartridge::Flash::readHalf(u32 address) -> u16 {
  address &= 0x1ffff;
  debug(unusual, "[Cartridge::Flash::readHalf] address=", hex(address, 6L), ", mode=", (u32)mode);

  if(address < 0x10000) {
    address &= 0xffff;
    if (mode == Mode::Read)   return readData(address);
    if (mode == Mode::Status) return readStatus(address);
  }

  return 0;
}

auto Cartridge::Flash::writeHalf(u32 address, u16 data) -> void {
  address &= 0x1ffff;
  debug(unusual, "[Cartridge::Flash::writeHalf] ignoring write to address=", hex(address, 6L), ", mode=", (u32)mode);

  if(address < 0x10000) {
    address &= 0xffff;
    if (mode == Mode::Write)  return writeData  (address, data);
    if (mode == Mode::Status) return writeStatus(address, data);
  }
  if(address == 0x10000) return writeCommand(data);
  if(address == 0x10002) return writeOffset (data);

}

auto Cartridge::Flash::writeCommand(u16 data) -> void {
  switch(data >> 8) {
  case 0x4b:  //set erase offset
    return;

  case 0x78:  //erase
    mode = Mode::Erase;
    status = 0x1111'8008'00c2'001dull;
    return;

  case 0xa5:  //set write offset
    status = 0x1111'8004'00c2'001dull;
    return;

  case 0xb4:  //write
    mode = Mode::Write;
    return;

  case 0xd2:  //execute
    if(mode == Mode::Erase) {
      for(u32 index = 0; index < 128; index += 2) {
        Memory::Writable::write<Half>(offset + index, 0xffff);
      }
    }
    if(mode == Mode::Write) {
      for(u32 index = 0; index < 128; index += 2) {
        u16 half = rdram.ram.read<Half>(source + index);
        Memory::Writable::write<Half>(offset + index, half);
      }
    }
    return;

  case 0xe1:  //status
    mode = Mode::Status;
    status = 0x1111'8001'00c2'001dull;
    return;

  case 0xf0:  //read
    mode = Mode::Read;
    status = 0x1111'8004'f000'001dull;
    return;

  default:
    debug(unusual, "[Cartridge::Flash::writeHalf] command=", hex(data, 4L));
    return;
  }
}

auto Cartridge::Flash::writeOffset(u16 data) -> void {
    offset = data * 128;
}

auto Cartridge::Flash::readData(u16 address) -> u16 {
  return Memory::Writable::read<Half>(address);
}

auto Cartridge::Flash::writeData(u16 address, u16 data) -> void {
  //writes are deferred until the flash execute command is sent later
  source = pi.io.dramAddress;
}

auto Cartridge::Flash::readStatus(u16 address) -> u16 {
  switch(address & 6) { default:
  case 0: return status >> 48;
  case 2: return status >> 32;
  case 4: return status >> 16;
  case 6: return status >>  0;
  }
}

auto Cartridge::Flash::writeStatus(u16 address, u16 data) -> void {
  debug(unusual, "[Cartridge::Flash::writeStatus] ignore writes to status: data=", hex(data, 4L));
}
