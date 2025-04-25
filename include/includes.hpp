#pragma once

#include <iostream>
#include <utility>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <signal.h>
#include <setjmp.h>
#include <fstream>
#include <sstream>

#if defined(_WIN32)
  #include <windows.h>
#elif defined(__MSYS__)
  #include <windows.h>
  #include <unistd.h>
#elif defined(__APPLE__)
  #include <unistd.h>
  #include <mach/mach.h>
  #include <mach/mach_vm.h>
  #include <mach/vm_region.h>
  #include <sys/mman.h>
  #include <pthread.h>
#elif defined(__linux__)
  #include <unistd.h>
#endif

std::pair<void*, void*> get_stack_bounds();
std::pair<void*, void*> get_heap_bounds();
void try_access_memory(uintptr_t addr);

#ifndef _WIN32
void segv_handler(int sig);
#endif
