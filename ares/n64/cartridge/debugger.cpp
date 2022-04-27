auto Cartridge::Debugger::load(Node::Object parent) -> void {
  memory.rom = parent->append<Node::Debugger::Memory>("Cartridge ROM");
  memory.rom->setSize(cartridge.rom.size);
  memory.rom->setRead([&](u32 address) -> u8 {
    return cartridge.rom.PIDeviceReadable::Readable::read<Byte>(address);
  });
  memory.rom->setWrite([&](u32 address, u8 data) -> void {
    return cartridge.rom.PIDeviceReadable::Readable::write<Byte>(address, data);
  });

  if(cartridge.ram) {
    memory.ram = parent->append<Node::Debugger::Memory>("Cartridge SRAM");
    memory.ram->setSize(cartridge.ram.size);
    memory.ram->setRead([&](u32 address) -> u8 {
      return cartridge.ram.PIDeviceWritable::Writable::read<Byte>(address);
    });
    memory.ram->setWrite([&](u32 address, u8 data) -> void {
      return cartridge.ram.PIDeviceWritable::Writable::write<Byte>(address, data);
    });
  }

  if(cartridge.eeprom) {
    memory.eeprom = parent->append<Node::Debugger::Memory>("Cartridge EEPROM");
    memory.eeprom->setSize(cartridge.eeprom.size);
    memory.eeprom->setRead([&](u32 address) -> u8 {
      return cartridge.eeprom.read<Byte>(address);
    });
    memory.eeprom->setWrite([&](u32 address, u8 data) -> void {
      return cartridge.eeprom.write<Byte>(address, data);
    });
  }

  if(cartridge.flash) {
    memory.flash = parent->append<Node::Debugger::Memory>("Cartridge Flash");
    memory.flash->setSize(cartridge.flash.size);
    memory.flash->setRead([&](u32 address) -> u8 {
      return cartridge.flash.Flash::PIDeviceWritable::Writable::read<Byte>(address);
    });
    memory.flash->setWrite([&](u32 address, u8 data) -> void {
      return cartridge.flash.Flash::PIDeviceWritable::Writable::write<Byte>(address, data);
    });
  }
}

auto Cartridge::Debugger::unload(Node::Object parent) -> void {
  parent->remove(memory.rom);
  parent->remove(memory.ram);
  parent->remove(memory.eeprom);
  parent->remove(memory.flash);
  memory.rom.reset();
  memory.ram.reset();
  memory.eeprom.reset();
  memory.flash.reset();
}
