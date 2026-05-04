#pragma once

#include "partner.hpp"
#include "rdb.hpp"

struct Devkit {
  auto load(Node::Object parent) -> void;
  auto unload() -> void;
  auto power(bool reset) -> void;
  auto serialize(serializer& s) -> void;

  struct Debugger {
    auto load(Node::Object parent) -> void;
    auto unload() -> void;
    auto setEnabled(bool enabled) -> void;
    auto notify(const string& message) -> void;

    struct Tracer {
      Node::Debugger::Tracer::Notification trace;
    } tracer;
  } debugger;

  bool enabled = true;
  DevkitRDB rdb;
  DevkitPartner partner;
};

extern Devkit devkit;
