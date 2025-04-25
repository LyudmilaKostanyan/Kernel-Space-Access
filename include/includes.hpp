#pragma once

#include <iostream>
#include <utility>
#include <cstdint>
#include <cstdlib>

#ifdef _WIN32
  #include <windows.h>
#else
  #include <unistd.h>
  #include <csignal>
  #include <csetjmp>
  #include <pthread.h>

  #if defined(__APPLE__)
    #include <mach/mach.h>
    #include <mach/mach_vm.h>
    #include <mach/vm_region.h>
  #endif
#endif

std::pair<void*, void*> get_stack_bounds();

std::pair<void*, void*> get_heap_bounds();

void try_access_memory(uintptr_t addr);