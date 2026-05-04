auto Devkit::Debugger::load(Node::Object parent) -> void {
  tracer.trace = parent->append<Node::Debugger::Tracer::Notification>("Trace", "Devkit");
  tracer.trace->setTerminal(true);
}

auto Devkit::Debugger::unload() -> void {
  tracer.trace.reset();
}

auto Devkit::Debugger::setEnabled(bool enabled_) -> void {
  if(!tracer.trace) return;
  tracer.trace->setTerminal(enabled_);
}

auto Devkit::Debugger::notify(const string& message) -> void {
  if(!tracer.trace) return;
  if(unlikely(tracer.trace->enabled())) tracer.trace->notify(message);
}
