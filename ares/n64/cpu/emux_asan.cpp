auto CPU::Xasan::ShadowEntry::create(n6 permissions, n3 poisonType, n4 tailInvalidBytes)
-> ShadowEntry {
  ShadowEntry e;
  e.setPermissions(permissions);
  e.setPoisonType(poisonType);
  e.setTailInvalidBytes(tailInvalidBytes);
  return e;
}

auto CPU::Xasan::poison(u32 paddr, u32 size, u8 poisonType) -> void {
  if(shadow.empty() || !size) return;
  if(poisonType > PoisonUnallocated) poisonType = PoisonUserPoisoned;
  u64 rangeStart = paddr;
  u64 rangeEnd = rangeStart + size;
  if(rangeEnd > RdramSize) rangeEnd = RdramSize;
  if(rangeEnd <= rangeStart) return;
  u32 first = rangeStart / Granule;
  u32 last = (rangeEnd - 1) / Granule;
  for(u32 g = first; g <= last && g < shadow.size(); g++) {
    u64 granuleStart = (u64)g * Granule;
    u64 granuleEnd = granuleStart + Granule;
    u64 segmentStart = rangeStart > granuleStart ? rangeStart : granuleStart;
    u64 segmentEnd = rangeEnd < granuleEnd ? rangeEnd : granuleEnd;
    if(segmentStart == granuleStart && segmentEnd == granuleEnd) {
      shadow[g] = ShadowEntry::create(n6(AllPermissions), poisonType, 0);
      continue;
    }
    if(segmentEnd == granuleEnd) {
      u8 tailInvalidBytes = granuleEnd - segmentStart;
      shadow[g] = ShadowEntry::create(n6(AllPermissions), poisonType, tailInvalidBytes);
      continue;
    }
    shadow[g] = ShadowEntry::create(n6(AllPermissions), poisonType, 0);
  }
}

auto CPU::Xasan::unpoison(u32 paddr, u32 size) -> void {
  if(shadow.empty() || !size) return;
  u64 rangeStart = paddr;
  u64 rangeEnd = rangeStart + size;
  if(rangeEnd > RdramSize) rangeEnd = RdramSize;
  if(rangeEnd <= rangeStart) return;
  u32 first = rangeStart / Granule;
  u32 last = (rangeEnd - 1) / Granule;
  for(u32 g = first; g <= last && g < shadow.size(); g++) {
    ShadowEntry entry = shadow[g];
    u64 granuleStart = (u64)g * Granule;
    u64 granuleEnd = granuleStart + Granule;
    u64 segmentStart = rangeStart > granuleStart ? rangeStart : granuleStart;
    u64 segmentEnd = rangeEnd < granuleEnd ? rangeEnd : granuleEnd;
    if(segmentStart == granuleStart && segmentEnd == granuleEnd) {
      shadow[g] = defaultEntry();
      continue;
    }
    if(segmentStart == granuleStart) {
      u8 poisonType = entry.poisonType();
      if(poisonType == PoisonAccessible) poisonType = PoisonUserPoisoned;
      u8 tailInvalidBytes = granuleEnd - segmentEnd;
      shadow[g] = ShadowEntry::create(n6(AllPermissions), poisonType, tailInvalidBytes);
      continue;
    }
    shadow[g] = defaultEntry();
  }
}

auto CPU::Xasan::check(u32 paddr, u32 size, u8 permissionMask, u8 accessType) const -> Fault {
  Fault fault;
  if(shadow.empty() || !size) return fault;
  u64 accessStart = paddr;
  u64 accessEnd = accessStart + size;
  if(accessEnd > RdramSize) accessEnd = RdramSize;
  if(accessEnd <= accessStart) return fault;
  u32 first = accessStart / Granule;
  u32 last = (accessEnd - 1) / Granule;
  for(u32 g = first; g <= last && g < shadow.size(); g++) {
    ShadowEntry entry = shadow[g];
    n6 permissions = entry.permissions();
    if((permissions & permissionMask) != permissionMask) {
      fault.poisonType = entry.poisonType();
      fault.accessType = accessType;
      fault.faultClass = FaultClassPermission;
      return fault;
    }
    u8 tailInvalidBytes = entry.tailInvalidBytes();
    if(tailInvalidBytes) {
      u64 granuleEnd = (u64)(g + 1) * Granule;
      u64 tailStart = granuleEnd - tailInvalidBytes;
      if(accessEnd > tailStart && accessStart < granuleEnd) {
        fault.poisonType = entry.poisonType();
        fault.accessType = accessType;
        fault.faultClass = FaultClassTail;
        return fault;
      }
      continue;
    }
    u8 poisonType = entry.poisonType();
    if(poisonType != PoisonAccessible) {
      fault.poisonType = poisonType;
      fault.accessType = accessType;
      fault.faultClass = FaultClassPoison;
      return fault;
    }
  }
  return fault;
}

auto CPU::Xasan::checkRead(CPU& self, const PhysAccess& access, u32 size) const -> bool {
  if(access.paddr >= RdramSize) return true;
  auto fault = check(access.paddr, size, PermissionCpuRead, AccessTypeCpuRead);
  if(!fault.active()) return true;
  self.xasanReport(false, access.vaddr, size, fault.poisonType, fault.accessType, fault.faultClass);
  if(self.emuxState.excMask.bit(ExceptionMaskBit)) {
    u32 faultData = ExceptionMaskBit;
    faultData |= (u32)fault.poisonType << 8;
    faultData |= (u32)fault.accessType << 16;
    faultData |= (u32)fault.faultClass << 24;
    self.emuxException(faultData);
    self.exception.emux();
  }
  return false;
}

auto CPU::Xasan::checkWrite(CPU& self, const PhysAccess& access, u32 size) const -> bool {
  if(access.paddr >= RdramSize) return true;
  auto fault = check(access.paddr, size, PermissionCpuWrite, AccessTypeCpuWrite);
  if(!fault.active()) return true;
  self.xasanReport(true, access.vaddr, size, fault.poisonType, fault.accessType, fault.faultClass);
  if(self.emuxState.excMask.bit(ExceptionMaskBit)) {
    u32 faultData = ExceptionMaskBit;
    faultData |= (u32)fault.poisonType << 8;
    faultData |= (u32)fault.accessType << 16;
    faultData |= (u32)fault.faultClass << 24;
    self.emuxException(faultData);
    self.exception.emux();
  }
  return false;
}
