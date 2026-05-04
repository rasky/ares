#include <n64/n64.hpp>

namespace ares::Nintendo64 {

Devkit devkit;
#include "debugger.cpp"
#include "rdb.cpp"
#include "partner.cpp"
#include "serialization.cpp"

auto Devkit::load(Node::Object parent) -> void {
  debugger.load(parent);
  debugger.setEnabled(enabled);
}

auto Devkit::unload() -> void {
  debugger.unload();
}

auto Devkit::power(bool reset) -> void {
  rdb.power(reset);
  partner.power(reset);
}

}
