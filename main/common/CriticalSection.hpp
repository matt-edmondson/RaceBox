// Copyright (c) Matthew Edmondson, 2025
// All rights reserved.
// Licensed under the MIT license.

#pragma once

// A minimal spinlock used to hand data from the NimBLE host task to the LVGL
// task. Sections guarded by this must stay short -- they are only ever used to
// copy a small struct or a couple of scalars.

#if __has_include("freertos/FreeRTOS.h")
  #include "freertos/FreeRTOS.h"
  #include "freertos/task.h"
  #define RACEBOX_HAVE_FREERTOS 1
#endif

namespace ktsu { namespace racebox { namespace common {

class SpinLock {
 public:
#ifdef RACEBOX_HAVE_FREERTOS
  void enter() { portENTER_CRITICAL(&mux_); }
  void exit() { portEXIT_CRITICAL(&mux_); }

 private:
  portMUX_TYPE mux_ = portMUX_INITIALIZER_UNLOCKED;
#else
  // Host / lint build is single threaded.
  void enter() {}
  void exit() {}
#endif
};

// RAII guard so no early return can leave the lock held.
class LockGuard {
 public:
  explicit LockGuard(SpinLock& lock) : lock_(lock) { lock_.enter(); }
  ~LockGuard() { lock_.exit(); }
  LockGuard(const LockGuard&) = delete;
  LockGuard& operator=(const LockGuard&) = delete;

 private:
  SpinLock& lock_;
};

} } } // namespaces
