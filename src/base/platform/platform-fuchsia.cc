// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <zircon/process.h>
#include <zircon/syscalls.h>

#include "src/base/macros.h"
#include "src/base/platform/platform-posix-time.h"
#include "src/base/platform/platform-posix.h"
#include "src/base/platform/platform.h"

namespace v8 {
namespace base {

TimezoneCache* OS::CreateTimezoneCache() {
  return new PosixDefaultTimezoneCache();
}

// static
void* OS::Allocate(void* address, size_t size, size_t alignment,
                   OS::MemoryPermission access) {
  // Currently we only support reserving memory.
  DCHECK_EQ(MemoryPermission::kNoAccess, access);
  size_t page_size = OS::AllocatePageSize();
  DCHECK_EQ(0, size % page_size);
  DCHECK_EQ(0, alignment % page_size);
  address = AlignedAddress(address, alignment);
  // Add the maximum misalignment so we are guaranteed an aligned base address.
  size_t request_size = size + (alignment - page_size);

  zx_handle_t vmo;
  if (zx_vmo_create(request_size, 0, &vmo) != ZX_OK) {
    return nullptr;
  }
  static const char kVirtualMemoryName[] = "v8-virtualmem";
  zx_object_set_property(vmo, ZX_PROP_NAME, kVirtualMemoryName,
                         strlen(kVirtualMemoryName));
  uintptr_t reservation;
  zx_status_t status = zx_vmar_map(zx_vmar_root_self(), 0, vmo, 0, request_size,
                                   0 /*no permissions*/, &reservation);
  // Either the vmo is now referenced by the vmar, or we failed and are bailing,
  // so close the vmo either way.
  zx_handle_close(vmo);
  if (status != ZX_OK) {
    return nullptr;
  }

  uint8_t* base = reinterpret_cast<uint8_t*>(reservation);
  uint8_t* aligned_base = RoundUp(base, alignment);

  // Unmap extra memory reserved before and after the desired block.
  if (aligned_base != base) {
    DCHECK_LT(base, aligned_base);
    size_t prefix_size = static_cast<size_t>(aligned_base - base);
    zx_vmar_unmap(zx_vmar_root_self(), reinterpret_cast<uintptr_t>(base),
                  prefix_size);
    request_size -= prefix_size;
  }

  size_t aligned_size = RoundUp(size, page_size);

  if (aligned_size != request_size) {
    DCHECK_LT(aligned_size, request_size);
    size_t suffix_size = request_size - aligned_size;
    zx_vmar_unmap(zx_vmar_root_self(),
                  reinterpret_cast<uintptr_t>(aligned_base + aligned_size),
                  suffix_size);
    request_size -= suffix_size;
  }

  DCHECK(aligned_size == request_size);
  return static_cast<void*>(aligned_base);
}

// static
void OS::Guard(void* address, size_t size) {
  CHECK_EQ(ZX_OK, zx_vmar_protect(zx_vmar_root_self(),
                                  reinterpret_cast<uintptr_t>(address), size,
                                  0 /*no permissions*/));
}

// static
bool OS::CommitRegion(void* address, size_t size, bool is_executable) {
  uint32_t prot = ZX_VM_FLAG_PERM_READ | ZX_VM_FLAG_PERM_WRITE |
                  (is_executable ? ZX_VM_FLAG_PERM_EXECUTE : 0);
  return zx_vmar_protect(zx_vmar_root_self(),
                         reinterpret_cast<uintptr_t>(address), size,
                         prot) == ZX_OK;
}

// static
bool OS::UncommitRegion(void* address, size_t size) {
  return zx_vmar_protect(zx_vmar_root_self(),
                         reinterpret_cast<uintptr_t>(address), size,
                         0 /*no permissions*/) == ZX_OK;
}

// static
bool OS::ReleaseRegion(void* address, size_t size) {
  return zx_vmar_unmap(zx_vmar_root_self(),
                       reinterpret_cast<uintptr_t>(address), size) == ZX_OK;
}

// static
bool OS::ReleasePartialRegion(void* address, size_t size) {
  return zx_vmar_unmap(zx_vmar_root_self(),
                       reinterpret_cast<uintptr_t>(address), size) == ZX_OK;
}

// static
bool OS::HasLazyCommits() {
  // TODO(scottmg): Port, https://crbug.com/731217.
  return false;
}

std::vector<OS::SharedLibraryAddress> OS::GetSharedLibraryAddresses() {
  CHECK(false);  // TODO(scottmg): Port, https://crbug.com/731217.
  return std::vector<SharedLibraryAddress>();
}

void OS::SignalCodeMovingGC() {
  CHECK(false);  // TODO(scottmg): Port, https://crbug.com/731217.
}

}  // namespace base
}  // namespace v8
