#include "includes.hpp"
#include <windows.h>

std::pair<void*, void*> get_stack_bounds() {
    MEMORY_BASIC_INFORMATION mbi;
    VirtualQuery(&mbi, &mbi, sizeof(mbi));
    void* stack_top    = mbi.BaseAddress;
    void* stack_bottom = mbi.AllocationBase;
    return { stack_bottom, stack_top };
}

std::pair<void*, void*> get_heap_bounds() {
    void* sample = HeapAlloc(GetProcessHeap(), 0, 1);
    MEMORY_BASIC_INFORMATION mbi;
    VirtualQuery(sample, &mbi, sizeof(mbi));
    void* heap_base = mbi.AllocationBase;
    void* heap_end  = static_cast<char*>(mbi.BaseAddress) + mbi.RegionSize;
    HeapFree(GetProcessHeap(), 0, sample);
    return { heap_base, heap_end };
}

void try_access_memory(uintptr_t addr) {
    std::cout << std::hex;
    __try {
        volatile int value = *reinterpret_cast<volatile int*>(addr);
        std::cout << "[*] Value at 0x" << addr << ": " << std::dec << value << "\n";
    }
    __except (GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION
              ? EXCEPTION_EXECUTE_HANDLER
              : EXCEPTION_CONTINUE_SEARCH) {
        std::cout << "[!] Access violation at 0x" << addr << "\n";
    }
}
