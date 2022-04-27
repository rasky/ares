struct Cartridge {
  Node::Peripheral node;
  VFS::Pak pak;
  Memory::PIDeviceReadable rom;
  Memory::PIDeviceWritable ram;
  Memory::Writable eeprom;
  struct Flash : Memory::PIDeviceWritable {
    //flash.cpp
    auto readHalf(u32 address) -> u16;
    auto writeHalf(u32 address, u16 data) -> void;

    auto writeCommand(u16 data) -> void;
    auto writeOffset(u16 data) -> void;
    auto readData(u16 address) -> u16;
    auto writeData(u16 address, u16 data) -> void;
    auto readStatus(u16 address) -> u16;
    auto writeStatus(u16 address, u16 data) -> void;

    enum class Mode : u32 { Idle, Erase, Write, Read, Status };
    Mode mode = Mode::Idle;
    u64  status = 0;
    u32  source = 0;
    u32  offset = 0;
  } flash;
  struct ISViewer : Memory::IO<ISViewer> {
    Memory::Writable ram;  //unserialized

    //isviewer.cpp
    auto readWord(u32 address) -> u32;
    auto writeWord(u32 address, u32 data) -> void;
  } isviewer;

  struct Debugger {
    //debugger.cpp
    auto load(Node::Object) -> void;
    auto unload(Node::Object) -> void;

    struct Memory {
      Node::Debugger::Memory rom;
      Node::Debugger::Memory ram;
      Node::Debugger::Memory eeprom;
      Node::Debugger::Memory flash;
    } memory;
  } debugger;

  auto title() const -> string { return information.title; }
  auto region() const -> string { return information.region; }
  auto cic() const -> string { return information.cic; }

  //cartridge.cpp
  auto allocate(Node::Port) -> Node::Peripheral;
  auto connect() -> void;
  auto disconnect() -> void;

  auto save() -> void;
  auto power(bool reset) -> void;

  //serialization.cpp
  auto serialize(serializer&) -> void;

private:
  struct Information {
    string title;
    string region;
    string cic;
  } information;
};

#include "slot.hpp"
extern Cartridge& cartridge;
