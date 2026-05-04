auto Devkit::serialize(serializer& s) -> void {
  s(enabled);
  s(rdb);
  s(partner);
}

auto DevkitRDB::serialize(serializer& s) -> void {
  s(fault.offset);
  for(auto& byte : fault.buffer) s(byte);
  s(debug.lengthOffset);
  for(auto& byte : debug.lengthBuffer) s(byte);
  s(debug.offset);
  s(debug.expected);
  for(auto& byte : debug.buffer) s(byte);
  s(log.countOffset);
  for(auto& byte : log.countBuffer) s(byte);
  s(log.offset);
  s(log.expected);
  for(auto& byte : log.buffer) s(byte);
  for(auto& word : host.queue) s(word);
  s(host.read);
  s(host.write);
  s(host.count);
  s(io.data);
  s(print.size);
  for(auto& c : print.buffer) s(c);
}

auto DevkitPartner::serialize(serializer& s) -> void {
  s(io.data);
  s(transfer.handshakePending);
  s(transfer.handshakeMode);
  s(transfer.handshakeLength);
  s(transfer.writeActive);
  s(transfer.writeExpected);
  s(transfer.writeOffset);
  for(auto& byte : transfer.writeData) s(byte);
  s(transfer.readActive);
  s(transfer.readOffset);
  s(transfer.readSource);
  for(auto& transferData : response.data) {
    for(auto& byte : transferData) s(byte);
  }
  for(auto& size : response.size) s(size);
  s(response.read);
  s(response.write);
  s(response.count);
  for(auto& word : queue.data) s(word);
  s(queue.read);
  s(queue.write);
  s(queue.count);

  if(s.reading()) {
    streamReset();
    transfer.readActive = false;
    transfer.readOffset = 0;
    transfer.readSource = ReadSourceNone;
  }
}
