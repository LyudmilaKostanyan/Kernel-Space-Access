#pragma once

#include <iostream>
#include <utility>
#include <cstdint>
#include <cstdlib>
#include <setjmp.h>
#include <signal.h>


std::pair<void*, void*> get_stack_bounds();

std::pair<void*, void*> get_heap_bounds();

void try_access_memory(uintptr_t addr);